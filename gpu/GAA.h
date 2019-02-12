#ifndef GAA_H
#define GAA_H

/*
 * GPU Aggregation Abstract Header
 */
#include "../tools/CudaHelper.h"
#include "../tools/tools.h"
#include <inttypes.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <cub/cub.cuh>

#define SHARED_MEM_BYTES 49152
#define THREADS_PER_BLOCK 256
#define U32_BYTES_PER_TUPLE 8
#define U64_BYTES_PER_TUPLE 12

#include <unistd.h>

#define VALIDATE true//ENABLE RESULT VALIDATION//

//VTA DEVICE MEMORY USAGE//
#define USE_DEVICE_MEM true

__constant__ float gpu_weights[NUM_DIMS];
__constant__ uint32_t gpu_query[NUM_DIMS];

template<class T>
__device__ T swap(T a, uint32_t stride, int dir){
	T b = __shfl_xor_sync(0xFFFFFFFF,a,stride);
	return ((a < b) == dir) ? b : a;
}
template<class T>
__device__ T rswap(T a, uint32_t stride, int dir){
	T b = __shfl_xor_sync(0xFFFFFFFF,a,stride);
	return ((a > b) == dir) ? b : a;
}

__device__ uint32_t bfe(uint32_t a, uint32_t i){
	return (a >> i) & 0x1;
}

/*
 * initialize global rearrange vector
 */
template<class Z>
__global__ void init_rvlocal(Z *dkeys_in, uint64_t n)
{
	uint64_t i = blockIdx.x * blockDim.x + threadIdx.x;
	if( i < n ){ dkeys_in[i] = i; }
}

/*
 * initialize first seen position
 */

template<class Z>
__global__ void init_first_seen_position(Z *rvector, uint64_t n)
{
	Z i = blockIdx.x * blockDim.x + threadIdx.x;
	if( i < n ){ rvector[i] = n; }
}

/*
 * find max rearrange value
 */
template<class Z>
__global__ void max_rvglobal(Z *rvector, Z *dkeys_in, uint64_t n)
{
	Z i = blockIdx.x * blockDim.x + threadIdx.x;
	if( i < n ){
		Z obj_id = dkeys_in[i];
		rvector[obj_id] = min(rvector[obj_id],i);
	}

}

void wait(){
	 do { std::cout << '\n' << "Press a key to continue..."; } while (std::cin.get() != '\n');
}

/*
 * Tuple structure
 */
template<class T,class Z>
struct ranked_tuple{
	ranked_tuple(){ tid = 0; score = 0; }
	ranked_tuple(Z t, T s){ tid = t; score = s; }
	Z tid;
	T score;
};

template<class T,class Z>
class MaxFirst{
	public:
		MaxFirst(){};

		bool operator() (const ranked_tuple<T,Z>& lhs, const ranked_tuple<T,Z>& rhs) const{
			return (lhs.score>rhs.score);
		}
};

template<class T, class Z>
static T compute_threshold(T* cdata, uint64_t n, uint64_t d, T *weights, uint32_t *query, uint64_t k){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;
	T *csvector =(T*)malloc(sizeof(T)*n);
	for(uint64_t i = 0; i < n; i++){
		T score = 0;
		for(uint64_t m = 0; m < d; m++){
			uint32_t a = query[m];
			score+=cdata[a*n + i] * weights[a];
		}
		csvector[i] = score;
		if(q.size() < k){
			q.push(ranked_tuple<T,Z>(i,score));
		}else if ( q.top().score < score ){
			q.pop();
			q.push(ranked_tuple<T,Z>(i,score));
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
static T find_threshold(T *cscores, uint64_t n, uint64_t k){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;
	for(uint64_t i = 0; i < n; i++){
		if(q.size() < k){
			q.push(ranked_tuple<T,Z>(i,cscores[i]));
		}else if ( q.top().score < cscores[i] ){
			q.pop();
			q.push(ranked_tuple<T,Z>(i,cscores[i]));
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
static T find_partial_threshold(T *cscores, uint64_t n, uint64_t k, bool type, uint32_t remainder){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;

	if(type){
		for(uint64_t j = 0; j < n; j+=4096){
			for(uint64_t i = 0; i < k;i++){
				if(q.size() < k){
					q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
				}else if ( q.top().score < cscores[i+j] ){
					q.pop();
					q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
				}
			}
		}
	}else{
		for(uint64_t j = 0; j < remainder; j++){
			if(q.size() < k){
				q.push(ranked_tuple<T,Z>(j,cscores[j]));
			}else if ( q.top().score < cscores[j] ){
				q.pop();
				q.push(ranked_tuple<T,Z>(j,cscores[j]));
			}
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "partial threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
static T find_remain_threshold(T *cscores, uint64_t n, uint64_t k, uint32_t remainder){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;

	for(uint64_t j = 0; j < n; j+=4096){
		for(uint64_t i = 0; i < remainder;i++){
			if(q.size() < k){
				q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
			}else if ( q.top().score < cscores[i+j] ){
				q.pop();
				q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
			}
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "remain threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
class GAA{
	public:
		GAA<T,Z>(uint64_t n, uint64_t d){
			this->n = n;
			this->d = d;
			this->cdata = NULL;
			this->gdata = NULL;
			this->cids = NULL;
			this->gids = NULL;
			this->cscores = NULL;
			this->gscores = NULL;
			this->cpu_threshold = 0;
			this->gpu_threshold = 0;

			this->using_dev_mem = false;

			this->iter = 1;
			this->parts = 0;
			this->block_size = 0;
			this->tt_init = 0;
			this->tt_processing = 0;
			this->pred_count = 0;
			this->tuple_count = 0;
			this->queries_per_second = 0;
			std::cout << std::fixed << std::setprecision(4);
		};

		~GAA<T,Z>(){
			if(this->cdata) cutil::safeCudaFreeHost(this->cdata,"free cdata"); //if(this->cdata != NULL){ cudaFreeHost(this->cdata); this->cdata = NULL; }
			if(this->cids) cutil::safeCudaFreeHost(this->cids,"free cids"); //if(this->cids != NULL){ cudaFreeHost(this->cids); this->cids = NULL; }
			if(this->cscores) cutil::safeCudaFreeHost(this->cscores,"free cscores"); //if(this->cscores != NULL){ cudaFreeHost(this->cscores); this->cscores = NULL; }

			if(this->gdata) cutil::safeCudaFree(this->gdata,"free gdata"); //if(this->gdata != NULL){ cudaFree(this->gdata); this->gdata = NULL; }
			if(this->gids) cutil::safeCudaFree(this->gids,"free gids"); //if(this->gids != NULL){ cudaFree(this->gids); this->gids = NULL; }
			if(this->gscores) cutil::safeCudaFree(this->gscores,"free gscores"); //if(this->gscores != NULL){ cudaFree(this->gscores); this->gscores = NULL; }
		};

		void findTopKtpac(uint64_t k,uint8_t qq, T *weights, uint32_t *attr);

		std::string get_algo(){ return this->algo; }
		T get_thres(){return this->threshold;}
		T*& get_cdata(){ return this->cdata; }
		void set_cdata(T *cdata){ this->cdata = cdata; }
		T*& get_gdata(){ return this->gdata; }
		void set_gdata(T *gdata){ this->gdata = gdata; }

		void set_iter(uint64_t iter){ this->iter = iter; }
		void set_partitions(uint64_t parts){ this->parts = parts; }

		void benchmark(uint64_t k,uint64_t qq){
			std::cout << std::fixed << std::setprecision(4);
			std::string msg = "| Benchmark for " + this->algo + "(" +
					std::to_string(this->n) + "," + std::to_string(qq) + "," + std::to_string(k)
					+ ")-(" +
					std::to_string(this->parts) + "," + std::to_string(this->block_size) + ") algorithm |";
			for(uint32_t i = 0; i < msg.size(); i++) std::cout << "-";
 			std::cout << std::endl << msg << std::endl;
 			for(uint32_t i = 0; i < msg.size(); i++) std::cout << "-";
 			std::cout << std::endl;

 			std::cout << "Accessing data from { " << (this->using_dev_mem ? "Device Memory" : "Host Memory")  << " }"<< std::endl;
			std::cout << "tt_init: " << this->tt_init << std::endl;
			std::cout << "tt_procesing: " << this->tt_processing/this->iter << std::endl;
			//std::cout << "tuples_per_second: " << (this->tt_processing == 0 ? 0 : WORKLOAD/(this->tt_processing/1000)) << std::endl;
			std::cout << "tuple_count: " << this->tuple_count << std::endl;
			std::cout << "cpu_threshold: " << this->cpu_threshold << std::endl;
			std::cout << "gpu_threshold: " << this->gpu_threshold << std::endl;
			//std::cout << "______________________________________________________" << std::endl;
			this->reset_stats();
		}

		void reset_stats(){
			this->tt_processing = 0;
			this->pred_count = 0;
			this->tuple_count = 0;
			this->queries_per_second = 0;
		}

		void copy_query(T *weights, uint32_t *query){
			this->weights = weights;
			this->query = query;
			cutil::cudaCheckErr(cudaMemcpyToSymbol(gpu_weights, weights, sizeof(T)*NUM_DIMS),"copy weights");//Initialize preference vector
			cutil::cudaCheckErr(cudaMemcpyToSymbol(gpu_query, query, sizeof(uint32_t)*NUM_DIMS),"copy query");//Initialize query vector
		}

	protected:
		std::string algo = "default";
		uint64_t n,d;// cardinality,dimensinality
		T *cdata;
		T *gdata;
		Z *cids;
		Z *gids;
		T *cscores;
		T *gscores;
		T cpu_threshold;
		T gpu_threshold;

		T *weights;
		uint32_t *query;

		uint64_t iter;//experiment count
		uint64_t parts;//partition number
		uint64_t block_size;// block_size
		double tt_init;
		double tt_processing;
		uint64_t pred_count;//count predicate evaluations
		uint64_t tuple_count;//count predicate evaluations
		uint64_t queries_per_second;

		bool using_dev_mem;

		Time<msecs> t;
};

template<class T>
__global__ void reduce_rebuild_atm_16(T *iscores, uint64_t n, uint64_t k, T *oscores){
	uint64_t i;
	__shared__ T buffer[256];
	T v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5 = 0, v6 = 0, v7 = 0;
	T v8 = 0, v9 = 0, vA = 0, vB = 0, vC = 0, vD = 0, vE = 0, vF = 0;

	i = (blockIdx.x << 12) + threadIdx.x;
	if(i<n) v0 = iscores[i];
	if(i+256<n) v1 = iscores[i+256];
	if(i+512<n) v2 = iscores[i+512];
	if(i+768<n) v3 = iscores[i+768];
	if(i+1024<n) v4 = iscores[i+1024];
	if(i+1280<n) v5 = iscores[i+1280];
	if(i+1536<n) v6 = iscores[i+1536];
	if(i+1792<n) v7 = iscores[i+1792];
	if(i+2048<n) v8 = iscores[i+2048];
	if(i+2304<n) v9 = iscores[i+2304];
	if(i+2560<n) vA = iscores[i+2560];
	if(i+2816<n) vB = iscores[i+2816];
	if(i+3072<n) vC = iscores[i+3072];
	if(i+3328<n) vD = iscores[i+3328];
	if(i+3584<n) vE = iscores[i+3584];
	if(i+3840<n) vF = iscores[i+3840];

	//4096 -> 2048
	v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
	v1 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v1, k),v1);
	v2 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v2, k),v2);
	v3 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v3, k),v3);
	v4 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v4, k),v4);
	v5 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v5, k),v5);
	v6 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v6, k),v6);
	v7 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v7, k),v7);
	v8 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v8, k),v8);
	v9 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v9, k),v9);
	vA = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vA, k),vA);
	vB = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vB, k),vB);
	vC = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vC, k),vC);
	vD = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vD, k),vD);
	vE = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vE, k),vE);
	vF = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vF, k),vF);
	v0 = (threadIdx.x & k) == 0 ? v0 : v1;
	v2 = (threadIdx.x & k) == 0 ? v2 : v3;
	v4 = (threadIdx.x & k) == 0 ? v4 : v5;
	v6 = (threadIdx.x & k) == 0 ? v6 : v7;
	v8 = (threadIdx.x & k) == 0 ? v8 : v9;
	vA = (threadIdx.x & k) == 0 ? vA : vB;
	vC = (threadIdx.x & k) == 0 ? vC : vD;
	vE = (threadIdx.x & k) == 0 ? vE : vF;
	uint32_t laneId = threadIdx.x;
	uint32_t level = k>>1, step, dir;
	for(step = level; step > 0; step = step >> 1){
		dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
		v0 = swap(v0,step,dir);
		v2 = swap(v2,step,dir);
		v4 = swap(v4,step,dir);
		v6 = swap(v6,step,dir);
		v8 = swap(v8,step,dir);
		vA = swap(vA,step,dir);
		vC = swap(vC,step,dir);
		vE = swap(vE,step,dir);
	}

	//2048 -> 1024
	v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
	v2 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v2, k),v2);
	v4 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v4, k),v4);
	v6 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v6, k),v6);
	v8 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v8, k),v8);
	vA = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vA, k),vA);
	vC = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vC, k),vC);
	vE = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vE, k),vE);
	v0 = (threadIdx.x & k) == 0 ? v0 : v2;
	v4 = (threadIdx.x & k) == 0 ? v4 : v6;
	v8 = (threadIdx.x & k) == 0 ? v8 : vA;
	vC = (threadIdx.x & k) == 0 ? vC : vE;
	for(step = level; step > 0; step = step >> 1){
		dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
		v0 = swap(v0,step,dir);
		v4 = swap(v4,step,dir);
		v8 = swap(v8,step,dir);
		vC = swap(vC,step,dir);
	}

	//1024 -> 512
	v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
	v4 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v4, k),v4);
	v8 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v8, k),v8);
	vC = fmaxf(__shfl_xor_sync(0xFFFFFFFF, vC, k),vC);
	v0 = (threadIdx.x & k) == 0 ? v0 : v4;
	v8 = (threadIdx.x & k) == 0 ? v8 : vC;
	for(step = level; step > 0; step = step >> 1){
		dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
		v0 = swap(v0,step,dir);
		v8 = swap(v8,step,dir);
	}

	//512 -> 256
	v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
	v8 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v8, k),v8);
	v0 = (threadIdx.x & k) == 0 ? v0 : v8;
	for(step = level; step > 0; step = step >> 1){
		dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
		v0 = swap(v0,step,dir);
	}

	buffer[threadIdx.x] = v0;
	__syncthreads();
	if(threadIdx.x < 32)
	{
		v0 = buffer[threadIdx.x];
		v1 = buffer[threadIdx.x+32];
		v2 = buffer[threadIdx.x+64];
		v3 = buffer[threadIdx.x+96];
		v4 = buffer[threadIdx.x+128];
		v5 = buffer[threadIdx.x+160];
		v6 = buffer[threadIdx.x+192];
		v7 = buffer[threadIdx.x+224];

		/*
		 * 256->128
		 */
		for(step = level; step > 0; step = step >> 1){
			dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
			v0 = swap(v0,step,dir);
			v1 = swap(v1,step,dir);
			v2 = swap(v2,step,dir);
			v3 = swap(v3,step,dir);
			v4 = swap(v4,step,dir);
			v5 = swap(v5,step,dir);
			v6 = swap(v6,step,dir);
			v7 = swap(v7,step,dir);
		}
		v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
		v1 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v1, k),v1);
		v2 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v2, k),v2);
		v3 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v3, k),v3);
		v4 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v4, k),v4);
		v5 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v5, k),v5);
		v6 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v6, k),v6);
		v7 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v7, k),v7);
		v0 = (threadIdx.x & k) == 0 ? v0 : v1;
		v2 = (threadIdx.x & k) == 0 ? v2 : v3;
		v4 = (threadIdx.x & k) == 0 ? v4 : v5;
		v6 = (threadIdx.x & k) == 0 ? v6 : v7;

		/*
		 * 128->64
		 */
		for(step = level; step > 0; step = step >> 1){
			dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
			v0 = swap(v0,step,dir);
			v2 = swap(v2,step,dir);
			v4 = swap(v4,step,dir);
			v6 = swap(v6,step,dir);
		}
		v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
		v2 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v2, k),v2);
		v4 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v4, k),v4);
		v6 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v6, k),v6);
		v0 = (threadIdx.x & k) == 0 ? v0 : v2;
		v4 = (threadIdx.x & k) == 0 ? v4 : v6;

		/*
		 * 64->32
		 */
		for(step = level; step > 0; step = step >> 1){
			dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
			v0 = swap(v0,step,dir);
			v4 = swap(v4,step,dir);
		}
		v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
		v4 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v4, k),v4);
		v0 = (threadIdx.x & k) == 0 ? v0 : v4;

		/*
		 * 32->16
		 */
		for(step = level; step > 0; step = step >> 1){
			dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
			v0 = swap(v0,step,dir);
		}
		v0 = fmaxf(__shfl_xor_sync(0xFFFFFFFF, v0, k),v0);
		v0 = (threadIdx.x & k) == 0 ? v0 : 0;

		/*
		* Sort 16
		*/
		for(level = k; level < 32; level = level << 1){
			for(step = level; step > 0; step = step >> 1){
				dir = bfe(laneId,__ffs(level))^bfe(laneId,__ffs(step>>1));
				v0 = rswap(v0,step,dir);
			}
		}

		if(threadIdx.x < k)
		{
			i = blockIdx.x * k;
			if((blockIdx.x & 0x1) == 0) oscores[i + threadIdx.x] = v0; else oscores[i + (threadIdx.x ^ (k-1))] = v0;
		}
	}
}

#endif
