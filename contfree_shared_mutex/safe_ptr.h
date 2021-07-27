#pragma once
#ifndef SAFE_PTR_H
#define SAFE_PTR_H

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <condition_variable>
#include <array>
#include <sstream>
#include <cassert>
#include <random>
#include <iomanip>
#include <algorithm>

//#define SHARED_MTX   // C++14

#ifdef SHARED_MTX
#include <shared_mutex>   // C++14
#endif

namespace sf {


    // contention free shared mutex (same-lock-type is recursive for X->X, X->S or S->S locks), but (S->X - is UB)
    template<unsigned contention_free_count = 36, bool shared_flag = false>
    class contention_free_shared_mutex {
		std::atomic<bool> want_x_lock;
        //struct cont_free_flag_t { alignas(std::hardware_destructive_interference_size) std::atomic<int> value; cont_free_flag_t() { value = 0; } }; // C++17
		struct cont_free_flag_t { char tmp[60]; std::atomic<int> value; cont_free_flag_t() { value = 0; } };   // tmp[] to avoid false sharing
        typedef std::array<cont_free_flag_t, contention_free_count> array_slock_t;
        
		const std::shared_ptr<array_slock_t> shared_locks_array_ptr;  // 0 - unregistred, 1 registred & free, 2... - busy
		char avoid_falsesharing_1[64];

        array_slock_t &shared_locks_array;
		char avoid_falsesharing_2[64];

		int recursive_xlock_count;


		enum index_op_t { unregister_thread_op, get_index_op, register_thread_op };

#if (_WIN32 && _MSC_VER < 1900) // only for MSVS 2013
        typedef int64_t thread_id_t;
		std::atomic<thread_id_t> owner_thread_id;
        std::array<int64_t, contention_free_count> register_thread_array;
        int64_t get_fast_this_thread_id() {
            static __declspec(thread) int64_t fast_this_thread_id = 0;  // MSVS 2013 thread_local partially supported - only POD
            if (fast_this_thread_id == 0) {
                std::stringstream ss;
                ss << std::this_thread::get_id();   // https://connect.microsoft.com/VisualStudio/feedback/details/1558211
                fast_this_thread_id = std::stoll(ss.str());
            }
            return fast_this_thread_id;
        }

		int get_or_set_index(index_op_t index_op = get_index_op, int set_index = -1) {
			if (index_op == get_index_op) {  // get index
				auto const thread_id = get_fast_this_thread_id();

				for (size_t i = 0; i < register_thread_array.size(); ++i) {
					if (register_thread_array[i] == thread_id) {
						set_index = i;   // thread already registred                
						break;
					}
				}
			}
			else if (index_op == register_thread_op) {  // register thread
				register_thread_array[set_index] = get_fast_this_thread_id();
			}
			return set_index;
		}

#else
		typedef std::thread::id thread_id_t;
		std::atomic<std::thread::id> owner_thread_id;
		std::thread::id get_fast_this_thread_id() { return std::this_thread::get_id(); }

        struct unregister_t {
            int thread_index;
            std::shared_ptr<array_slock_t> array_slock_ptr;
            unregister_t(int index, std::shared_ptr<array_slock_t> const& ptr) : thread_index(index), array_slock_ptr(ptr) {}
            unregister_t(unregister_t &&src) : thread_index(src.thread_index), array_slock_ptr(std::move(src.array_slock_ptr)) {}
            ~unregister_t() { if (array_slock_ptr.use_count() > 0) (*array_slock_ptr)[thread_index].value--; }
        };

        int get_or_set_index(index_op_t index_op = get_index_op, int set_index = -1) {
            thread_local static std::unordered_map<void *, unregister_t> thread_local_index_hashmap;
            // get thread index - in any cases. Use &shared_locks_array as key ("this" may get recycled if contfree-mtx under "it" was deleted)
            auto it = thread_local_index_hashmap.find(&shared_locks_array);
            if (it != thread_local_index_hashmap.cend())
                set_index = it->second.thread_index;

            if (index_op == unregister_thread_op) {  // unregister thread
                if (shared_locks_array[set_index].value == 1) // if isn't shared_lock now
                    thread_local_index_hashmap.erase(&shared_locks_array);
                else
                    return -1;
            }
            else if (index_op == register_thread_op) {  // register thread
                thread_local_index_hashmap.emplace(&shared_locks_array, unregister_t(set_index, shared_locks_array_ptr));

                // remove info about deleted contfree-mutexes
                for (auto it = thread_local_index_hashmap.begin(), ite = thread_local_index_hashmap.end(); it != ite;) {
                    if (it->second.array_slock_ptr->at(it->second.thread_index).value < 0)    // if contfree-mtx was deleted
                        it = thread_local_index_hashmap.erase(it);
                    else
                        ++it;
                }
            }
            return set_index;
        }

#endif

        public:
            contention_free_shared_mutex() :
                shared_locks_array_ptr(std::make_shared<array_slock_t>()), shared_locks_array(*shared_locks_array_ptr), want_x_lock(false), recursive_xlock_count(0),
				owner_thread_id(thread_id_t()) {}

            ~contention_free_shared_mutex() {
                for (auto &i : shared_locks_array) i.value = -1;
            }


            bool unregister_thread() { return get_or_set_index(unregister_thread_op) >= 0; }

            int register_thread() {
                int cur_index = get_or_set_index();

                if (cur_index == -1) {
                    if (shared_locks_array_ptr.use_count() <= (int)shared_locks_array.size())  // try once to register thread
                    {
                        for (size_t i = 0; i < shared_locks_array.size(); ++i) {
                            int unregistred_value = 0;
                            if (shared_locks_array[i].value == 0)
                                if (shared_locks_array[i].value.compare_exchange_strong(unregistred_value, 1)) {
                                    cur_index = i;
                                    get_or_set_index(register_thread_op, cur_index);   // thread registred success
                                    break;
                                }
                        }
                        //std::cout << "\n thread_id = " << std::this_thread::get_id() << ", register_thread_index = " << cur_index <<
                        //    ", shared_locks_array[cur_index].value = " << shared_locks_array[cur_index].value << std::endl;
                    }
                }
                return cur_index;
            }

            void lock_shared() {
                int const register_index = register_thread();

                if (register_index >= 0) {
                    int recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);
                    assert(recursion_depth >= 1);

                    if (recursion_depth > 1)
                        shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_release); // if recursive -> release
                    else {
                        shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_seq_cst); // if first -> sequential
                        while (want_x_lock.load(std::memory_order_seq_cst)) {
                            shared_locks_array[register_index].value.store(recursion_depth, std::memory_order_seq_cst);
                            for (volatile size_t i = 0; want_x_lock.load(std::memory_order_seq_cst); ++i) 
								if (i % 100000 == 0) std::this_thread::yield();
                            shared_locks_array[register_index].value.store(recursion_depth + 1, std::memory_order_seq_cst);
                        }
                    }
                    // (shared_locks_array[register_index] == 2 && want_x_lock == false) ||     // first shared lock
                    // (shared_locks_array[register_index] > 2)                                 // recursive shared lock
                }
                else {
					if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
						size_t i = 0;
						for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
							if (++i % 100000 == 0) std::this_thread::yield();
						owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);
					}
					++recursive_xlock_count;
                }
            }

            void unlock_shared() {
                int const register_index = get_or_set_index();

                if (register_index >= 0) {
                    int const recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);
                    assert(recursion_depth > 1);

                    shared_locks_array[register_index].value.store(recursion_depth - 1, std::memory_order_release);
                }
                else {
					if (--recursive_xlock_count == 0) {
						owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
						want_x_lock.store(false, std::memory_order_release);
					}
                }
            }

            void lock() {
                // forbidden upgrade S-lock to X-lock - this is an excellent opportunity to get deadlock
                int const register_index = get_or_set_index();
                if (register_index >= 0)
                    assert(shared_locks_array[register_index].value.load(std::memory_order_acquire) == 1);

				if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
					size_t i = 0;
					for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
						if (++i % 1000000 == 0) std::this_thread::yield();

					owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);

					for (auto &i : shared_locks_array)
						while (i.value.load(std::memory_order_seq_cst) > 1);
				}

				++recursive_xlock_count;
            }

            void unlock() {
                assert(recursive_xlock_count > 0);
				if (--recursive_xlock_count == 0) {
					owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
					want_x_lock.store(false, std::memory_order_release);
				}
            }
    };

    template<typename mutex_t>
    struct shared_lock_guard {
        mutex_t &ref_mtx;
        shared_lock_guard(mutex_t &mtx) : ref_mtx(mtx) { ref_mtx.lock_shared(); }
        ~shared_lock_guard() { ref_mtx.unlock_shared(); }
    };

    using default_contention_free_shared_mutex = contention_free_shared_mutex<>;
    // ---------------------------------------------------------------
}


#endif // #ifndef SAFE_PTR_H
