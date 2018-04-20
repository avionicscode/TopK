#include<limits>
#include <stdio.h>
#include <cstdint>
#include <stdio.h>
#include <tmmintrin.h>
#include <immintrin.h>

#include "../validation/bench_cpu.h"


int main(int argc, char **argv){
	ArgParser ap;
	ap.parseArgs(argc,argv);
	if(!ap.exists("-f")){
		std::cout << "Missing file input!!!" << std::endl;
		exit(1);
	}

	if(!ap.exists("-d")){
		std::cout << "Missing d!!!" << std::endl;
		exit(1);
	}

	if(!ap.exists("-n")){
		std::cout << "Missing n!!!" << std::endl;
		exit(1);
	}

	uint64_t n = ap.getInt("-n");
	uint64_t d = ap.getInt("-d");
	uint64_t nu;
	if(!ap.exists("-nu")){
		nu = n;
	}else{
		nu = ap.getInt("-nu");
	}

	uint64_t nl;
	if(!ap.exists("-nl")){
		nl = n;
	}else{
		nl = ap.getInt("-nl");
	}

	if (TA_B == 1){ bench_ta(ap.getString("-f"),n,d,KKS,KKE); }
	if (TPAr_B == 1){ bench_tpar(ap.getString("-f"),n,d,KKS,KKE); }
	if (TPAc_B == 1){ bench_tpac(ap.getString("-f"),n,d,KKS,KKE);	}
	if (VTA_B == 1){ bench_vta(ap.getString("-f"),n,d,KKS,KKE); }
	if (PTA_B == 1){ bench_pta(ap.getString("-f"),n,d,KKS,KKE); }

	//bench_sla(ap.getString("-f"),n,d,K);
	//test_dt();
	//bench_vta(ap.getString("-f"),n,d,K);

	return 0;
}
