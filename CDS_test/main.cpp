#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>

#include "safe_ptr.h"

using namespace sf;

struct field_t { int money, time; field_t(int m, int t) : money(m), time(t) {} field_t() : money(0), time(0) {} };
typedef safe_obj<field_t, spinlock_t> safe_obj_field_t;


#define CDS_THREADING_AUTODETECT    // http://libcds.sourceforge.net/doc/cds-api/namespacecds_1_1threading.html#_details

#include <cds/init.h>       // for cds::Initialize and cds::Terminate
#include <cds/container/skip_list_map_rcu.h>    // cds::container::SkipListMap<>
#include <cds/container/bronson_avltree_map_rcu.h>  // cds::container::BronsonAVLTreeMap<>
#include <cds/urcu/general_buffered.h>   // gc<general_buffered> - general purpose RCU with deferred (buffered) reclamation
typedef cds::urcu::gc< cds::urcu::general_buffered<> >  rcu_gpb;    // high performance (higher than other 4 RCU implementations)


// only for measure latency (is not used for MOPS)
safe_ptr<std::vector<double>> safe_vec_max_latency;
static const size_t median_array_size = 1000000;
safe_ptr<std::vector<double>> safe_vec_median_latency;


enum { insert_op, delete_op, read_op };
std::uniform_int_distribution<size_t> percent_distribution(1, 100);    // 1 - 100 %


template<typename T>
void benchmark_mutex_map(T &test_map, std::mutex &mtx,
	size_t const iterations_count, size_t const percent_write, const bool measure_latency = false)
{
	const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<size_t> index_distribution(0, test_map.size() - 1);
	std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
	double max_time = 0;
	std::vector<double> median_arr;

	for (size_t i = 0; i < iterations_count; ++i) {
		int const rnd_index = (int)index_distribution(generator);
		bool const write_flag = (percent_distribution(generator) < percent_write);
		int const num_op = (write_flag) ? i % 2 : read_op;   // (insert_op, delete_op), read_op

		if (measure_latency) {
			hrc_end = std::chrono::high_resolution_clock::now();
			const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
			max_time = std::max(max_time, cur_time);
			if (median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
			if (i < median_arr.size()) median_arr[i] = cur_time;
			hrc_start = std::chrono::high_resolution_clock::now();
		}

		std::lock_guard<std::mutex> lock(mtx);
		bool success_op;
		switch (num_op) {
		case insert_op:			
			test_map.emplace(rnd_index, field_t(rnd_index, rnd_index));
			break;
		case delete_op:
			success_op = test_map.erase(rnd_index);
			break;
		case read_op: {
			auto it = test_map.find(rnd_index);
			if (it != test_map.cend()) {
				auto &field = it->second;
				volatile int money = field.money;    // get value
				field.money += 10;                   // update value
			}
		}
					  break;
		default: std::cout << "\n wrong way! \n";  break;
		}
	}

	safe_vec_max_latency->push_back(max_time);
	safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}


template<typename key_type = int, typename mapped_type = field_t>
struct get_n_update_functor_key_val {
	void operator()(key_type const& key, mapped_type& val) { volatile int money = val.money; val.money += 10; }
	void operator()(std::pair<const key_type, mapped_type> &key_val) { volatile int money = key_val.second.money; key_val.second.money += 10; }
};

template<typename T = cds::container::BronsonAVLTreeMap< rcu_gpb, int, int >>
void benchmark_cds_map(T &test_map,
    size_t const iterations_count, size_t const percent_write, const bool measure_latency = false)
{
    cds::threading::Manager::attachThread();

    const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<size_t> index_distribution(0, test_map.size() - 1);
    std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
    double max_time = 0;
    std::vector<double> median_arr;

    for (size_t i = 0; i < iterations_count; ++i) {
        int const rnd_index = (int)index_distribution(generator);
        bool const write_flag = (percent_distribution(generator) < percent_write);
        int const num_op = (write_flag) ? i % 2 : read_op;   // (insert_op, delete_op), read_op

        if (measure_latency) {
            hrc_end = std::chrono::high_resolution_clock::now();
            const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
            max_time = std::max(max_time, cur_time);
            if (median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
            if (i < median_arr.size()) median_arr[i] = cur_time;
            hrc_start = std::chrono::high_resolution_clock::now();
        }

        bool success_op;
        switch (num_op) {
        case insert_op:
            test_map.emplace(rnd_index, field_t(rnd_index, rnd_index));
            break;
        case delete_op:
            success_op = test_map.erase(rnd_index);
            break;
        case read_op:
            success_op = test_map.find(rnd_index, get_n_update_functor_key_val<>());
            break;
        default: std::cout << "\n wrong way! \n";  break;
        }
    }
    
    safe_vec_max_latency->push_back(max_time);
    safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
    cds::threading::Manager::detachThread();
}

template<typename T>
void benchmark_safe_map(T &test_map,
    size_t const iterations_count, size_t const percent_write, const bool measure_latency = false)
{
    const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<size_t> index_distribution(0, test_map->size() - 1);
    std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
    double max_time = 0;
    std::vector<double> median_arr;

    for (size_t i = 0; i < iterations_count; ++i) {
        int const rnd_index = (int)index_distribution(generator);
        bool const write_flag = (percent_distribution(generator) < percent_write);
        int const num_op = (write_flag) ? i % 2 : read_op;   // (insert_op, delete_op), read_op

        if (measure_latency) {
            hrc_end = std::chrono::high_resolution_clock::now();
            const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
            max_time = std::max(max_time, cur_time);
            if (median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
            if (i < median_arr.size()) median_arr[i] = cur_time;
            hrc_start = std::chrono::high_resolution_clock::now();
        }

        bool success_op;
        switch (num_op) {
        case insert_op:
            slock_safe_ptr(test_map)->find(rnd_index);  // find for pre-cache to L1 with temprorary S-lock
            test_map->emplace(rnd_index, field_t(rnd_index, rnd_index));
            break;
        case delete_op:
            slock_safe_ptr(test_map)->find(rnd_index);  // find for pre-cache to L1 with temprorary S-lock
            success_op = test_map->erase(rnd_index);
            break;
        case read_op: {
            auto s_safe_map = slock_safe_ptr(test_map); // S-lock on Table (must necessarily be)
            auto it = s_safe_map->find(rnd_index);
            if (it != s_safe_map->cend()) {
                auto x_field = xlock_safe_ptr(it->second);
                volatile int money = x_field->money;    // get value
                x_field->money += 10;                   // update value
            }
        }
            break;
        default: std::cout << "\n wrong way! \n";  break;
        }
    }

    safe_vec_max_latency->push_back(max_time);
    safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}


template<typename T>
void benchmark_map_partitioned(T &test_map,
    size_t const iterations_count, size_t const percent_write, const bool measure_latency = false)
{
    const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<size_t> index_distribution(0, test_map.size() - 1);
    std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
    double max_time = 0;
    std::vector<double> median_arr;

    for (size_t i = 0; i < iterations_count; ++i) {
        int const rnd_index = (int)index_distribution(generator);
        bool const write_flag = (percent_distribution(generator) < percent_write);
        int const num_op = (write_flag) ? i % 2 : read_op;   // (insert_op, delete_op), read_op

        if (measure_latency) {
            hrc_end = std::chrono::high_resolution_clock::now();
            const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
            max_time = std::max(max_time, cur_time);
            if (median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
            if (i < median_arr.size()) median_arr[i] = cur_time;
            hrc_start = std::chrono::high_resolution_clock::now();
        }

        size_t errased_elements;
        switch (num_op) {
        case insert_op:
            test_map.emplace(rnd_index, field_t(rnd_index, rnd_index));
            break;
        case delete_op:
            errased_elements = test_map.erase(rnd_index);
            break;
        case read_op: {
			auto const& test_map_ro = test_map;
            auto slock_container = slock_safe_ptr(test_map_ro.part(rnd_index));    // S-lock on Partition of Table (must necessarily be)
            auto it = slock_container->find(rnd_index);
            if (it != slock_container->cend()) {
                auto x_field = xlock_safe_ptr(it->second);
                volatile int money = x_field->money;    // get value
                x_field->money += 10;                   // update value
            }
        }
            break;
        default: std::cout << "\n wrong way! \n";  break;
        }
    }

    safe_vec_max_latency->push_back(max_time);
    safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}

int main(int argc, char** argv)
{
    const size_t iterations_count = 5000000;    // operation of data exchange between threads
    const size_t container_size = 100000;       // elements in container
    size_t percent_write = 10;
    const bool measure_latency = false;         // measure latency time for each operation (Max, Median)

    std::vector<std::thread> vec_thread(std::thread::hardware_concurrency());

	size_t custom_max_threads = vec_thread.size();
	if (argc >= 2) {
		custom_max_threads = std::stoi(std::string(argv[1]));		// max threads
	}

    std::cout << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "Benchmark thread-safe ORDERED associative containers with size = " << container_size << std::endl;
    std::cout << "Threads = " << vec_thread.size() << ", iterations per thread = " << iterations_count << std::endl;
    std::cout << "Time & MOps - steady_clock, is_steady = " << std::chrono::steady_clock::is_steady << ", num/den = " <<
        (double)std::chrono::steady_clock::period::num << " / " << std::chrono::steady_clock::period::den << std::endl;
    if(measure_latency)
        std::cout << "Latency     - highres_clock, is_steady = " << std::chrono::high_resolution_clock::is_steady << ", num/den = " <<
            (double)std::chrono::high_resolution_clock::period::num << " / " << std::chrono::high_resolution_clock::period::den << std::endl;


	// std::mutex & std::map
	std::map<int, field_t> std_map;
	std::mutex mutex_map;


	// CDS
	// http://libcds.sourceforge.net/doc/cds-api/classcds_1_1container_1_1_bronson_a_v_l_tree_map_3_01cds_1_1urcu_1_1gc_3_01_r_c_u_01_4_00_01_key_00_01_t_00_01_traits_01_4.html#cds_container_BronsonAVLTreeMap_rcu
	// This is a concurrent AVL tree algorithm that uses hand-over-hand optimistic validation, 
	// a concurrency control mechanism for searching and navigating a binary search tree.
	cds::container::BronsonAVLTreeMap< rcu_gpb, int, field_t > branson_avltree_map;

	// http://libcds.sourceforge.net/doc/cds-api/classcds_1_1container_1_1_skip_list_map_3_01cds_1_1urcu_1_1gc_3_01_r_c_u_01_4_00_01_key_00_01_t_00_01_traits_01_4.html
	// A skip-list is a probabilistic data structure that provides expected 
	// logarithmic time search without the need of rebalance.
	// The class supports a forward iterator (iterator and const_iterator). The iteration is ordered. You may iterate over skip-list set items only under RCU lock. Only in this case the iterator is thread-safe since while RCU is locked any set's item cannot be reclaimed.
	// The requirement of RCU lock during iterating means that deletion of the elements(i.e.erase) is not possible.
	cds::container::SkipListMap< rcu_gpb, int, field_t > skiplist_map;


	// SAFE_PTR
	// thread-safe ordered std::map by using execute around pointer idiom with contention-free shared-lock
	contfree_safe_ptr< std::map<int, safe_obj_field_t> > safe_map_contfree;

	// thread-safe custom partitioned ordered-map based on std::map by using execute around pointer idiom with contention-free shared-lock
	safe_map_partitioned_t<int, safe_obj_field_t, contfree_safe_ptr> safe_map_part_contfree(0, container_size, container_size / 10); // from 0 to 100 000 by step 10 000
	

    // Initialize libcds
    cds::Initialize();
	{
        // Initialize Hazard Pointer singleton
        //cds::gc::HP hpGC;

        // Initialize general_buffered RCU
        rcu_gpb   gpbRCU;

        // If main thread uses lock-free containers
        // the main thread should be attached to libcds infrastructure
        cds::threading::Manager::attachThread();
        // Now you can use HP-based containers in the main thread


		for (size_t t_num = 1; t_num <= std::thread::hardware_concurrency(); t_num = t_num * 2)
		{
			t_num = std::min(t_num, custom_max_threads);

			vec_thread.resize(t_num);
			std::cout << "------------------------------------------------" << std::endl;
			std::cout << std::endl << "Current threads: " << t_num << std::endl;
			std::cout << std::endl;
			std::cout << percent_write << "\t % of write operations (1/2 insert, 1/2 delete), " << (100 - percent_write) << "% update" << std::endl;
			std::cout << "                                        (1 Operation latency, usec)" << std::endl;;
			std::cout << "               \t     time, sec \t MOps \t Median\t Min \t Max " << std::endl;
			std::cout << std::setprecision(3);

			std_map.clear();
			branson_avltree_map.clear();
			skiplist_map.clear();
			safe_map_contfree->clear();
			safe_map_part_contfree.clear();

			// init maps
			for (size_t i = 0; i < container_size; ++i)
			{
				std_map.emplace(i, field_t(i, i));
				branson_avltree_map.emplace(i, field_t(i, i));
				skiplist_map.emplace(i, field_t(i, i));
				safe_map_contfree->emplace(i, field_t(i, i));
				safe_map_part_contfree.emplace(i, field_t(i, i));
			}

			std::chrono::steady_clock::time_point steady_start, steady_end;
			double took_time = 0;


			std::cout << "std::mutex & std::map:";
			steady_start = std::chrono::steady_clock::now();
			for (auto &i : vec_thread)
				i = std::move(std::thread([&]()
			{
				benchmark_mutex_map(std_map, mutex_map, iterations_count, percent_write, measure_latency);

			}));
			for (auto &i : vec_thread) i.join();
			steady_end = std::chrono::steady_clock::now();
			took_time = std::chrono::duration<double>(steady_end - steady_start).count();
			std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
			if (measure_latency) {
				std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
				std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
					" \t " << (safe_vec_median_latency->at(vec_thread.size()) * 1000000) <<
					" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
			}
			std::cout << std::endl;
			safe_vec_max_latency->clear();
			safe_vec_median_latency->clear();


			std::cout << "BronsonAVLTreeMap:";
			steady_start = std::chrono::steady_clock::now();
			for (auto &i : vec_thread)
				i = std::move(std::thread([&]()
			{
				benchmark_cds_map(branson_avltree_map, iterations_count, percent_write, measure_latency);

			}));
			for (auto &i : vec_thread) i.join();
			steady_end = std::chrono::steady_clock::now();
			took_time = std::chrono::duration<double>(steady_end - steady_start).count();
			std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
			if (measure_latency) {
				std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
				std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
					" \t " << (safe_vec_median_latency->at(vec_thread.size()) * 1000000) <<
					" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
			}
			std::cout << std::endl;
			safe_vec_max_latency->clear();
			safe_vec_median_latency->clear();



			std::cout << "SkipListMap:      ";
			steady_start = std::chrono::steady_clock::now();
			for (auto &i : vec_thread)
				i = std::move(std::thread([&]()
			{
				benchmark_cds_map(skiplist_map, iterations_count, percent_write, measure_latency);

			}));
			for (auto &i : vec_thread) i.join();
			steady_end = std::chrono::steady_clock::now();
			took_time = std::chrono::duration<double>(steady_end - steady_start).count();
			std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
			if (measure_latency) {
				std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
				std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
					" \t " << (safe_vec_median_latency->at(vec_thread.size()) * 1000000) <<
					" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
			}
			std::cout << std::endl;
			safe_vec_max_latency->clear();
			safe_vec_median_latency->clear();



			std::cout << "safe_map_contfree:  ";
			steady_start = std::chrono::steady_clock::now();
			for (auto &i : vec_thread)
				i = std::move(std::thread([&]()
			{
				benchmark_safe_map(safe_map_contfree, iterations_count, percent_write, measure_latency);

			}));
			for (auto &i : vec_thread) i.join();
			steady_end = std::chrono::steady_clock::now();
			took_time = std::chrono::duration<double>(steady_end - steady_start).count();
			std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
			if (measure_latency) {
				std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
				std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
					" \t " << (safe_vec_median_latency->at(vec_thread.size()) * 1000000) <<
					" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
			}
			std::cout << std::endl;
			safe_vec_max_latency->clear();
			safe_vec_median_latency->clear();


			std::cout << "safe_map_part_contfree:";
			steady_start = std::chrono::steady_clock::now();
			for (auto &i : vec_thread)
				i = std::move(std::thread([&]()
			{
				benchmark_map_partitioned(safe_map_part_contfree, iterations_count, percent_write, measure_latency);

			}));
			for (auto &i : vec_thread) i.join();
			steady_end = std::chrono::steady_clock::now();
			took_time = std::chrono::duration<double>(steady_end - steady_start).count();
			std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count / (took_time * 1000000));
			if (measure_latency) {
				std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
				std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size() / 2) * 1000000) <<
					" \t " << (safe_vec_median_latency->at(vec_thread.size()) * 1000000) <<
					" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
			}
			std::cout << std::endl;
			safe_vec_max_latency->clear();
			safe_vec_median_latency->clear();			

			if (t_num >= custom_max_threads) break;
		}
        std::cout << "\n\n finished \n";
    }
    
    int b = getchar(); 

    // Terminate libcds
    cds::Terminate();
}