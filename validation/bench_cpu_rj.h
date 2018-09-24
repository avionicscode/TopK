#ifndef BENCH_CPU_RJ
#define BENCH_CPU_RJ

//#include "../cpu_rj/ARJ.h"
#include "../cpu_rj/HJR.h"

void test_bench(){
	uint32_t n0 = 1024, d0 = 2;
	uint32_t n1 = 1024*1024, d1 = 2;
	uint32_t k = 10;
	RankJoinInstance<uint32_t,float> rj_inst(n0,d0,n1,d1,k);
	GenData<uint32_t,float> gen_data(&rj_inst);

	rj_inst.sample();
}

void bench_no_part_hjr(){
	uint32_t n0 = 1*1024*1024, d0 = 2;
	uint32_t n1 = 64*1024*1024, d1 = 2;
	uint32_t k = 10;
	RankJoinInstance<uint32_t,float> rj_inst(n0,d0,n1,d1,k);
	GenData<uint32_t,float> gen_data(&rj_inst);
	HJR<uint32_t,float> hjr(&rj_inst);

	//rj_inst.sample();
	hjr.can_hash_join(); hjr.benchmark();
	hjr.nop_hash_join(); hjr.benchmark();
	hjr.prt_hash_join(); hjr.benchmark();
}
#endif

