CC=g++
#CC=icpc
NVCC=/usr/local/cuda-10.0/bin/nvcc
NVCC_INCLUDE=-I../cub-1.8.0/ -I/usr/local/cuda/include/
NVCC_LIBS=-L/usr/local/cuda-10.0/lib64/

#REORDER APP
CC_REORDER=input/main.cpp
CC_EXE_RE=reorder_run

#########
#Skyline#
DIMS=8
V=VERBOSE
DT=0
PROFILER=0

######
#TopK#
#QM 0:Reverse query attribute, 1:Forward query attributes
QM=0
#QD Dimension interval for testing
QD=1
#IMP 0:Scalar, 1:SIMD, 2:Threads
IMP=2
#ITER Testing iterations
ITER=1
#LD 0:load from file, 1: generate in memory
LD=0
#DISTR c:correlated i:independent a:anticorrelated
DISTR=1
#Multiple thread count
MQTHREADS=32
#Gather object evaluation statistics
STATS_EFF=true
#Choose workload for multi-query evaluation
WORKLOAD=32768

######CPU ALGORITHM######
#TA Benchmark
TA_B=1
#TPAc Benchmark
TPAc_B=1
#TPAr Benchmark
TPAr_B=1
#VTA Benchmark
VTA_B=1
#PTA Benchmark
PTA_B=1
#SLA Benchmark
SLA_B=1

######GPU ALGORITHM######
#BTA Benchmark
BTA_B=1
#GVTA Benchmark
GVTA_B=1
#GPTA Benchmark
GPTA_B=1

#Top-K
KKS=16
KKE=16

BENCH=-DTA_B=$(TA_B) -DTPAc_B=$(TPAc_B) -DTPAr_B=$(TPAr_B) -DVTA_B=$(VTA_B) -DPTA_B=$(PTA_B) -DSLA_B=$(SLA_B) -DMQTHREADS=$(MQTHREADS) -DSTATS_EFF=$(STATS_EFF) -DWORKLOAD=$(WORKLOAD)
GBENCH=-DBTA_B=$(BTA_B) -DGVTA_B=$(GVTA_B) -DGPTA_B=$(GPTA_B)

#CPU CONFIGURATION
CC_MAIN=cpu/main.cpp skyline/hybrid/hybrid.cpp input/randdataset-1.1.0/src/randdataset.c
CC_FLAGS=-std=c++11 -g
CC_EXE=cpu_run
CC_OPT_FLAGS_GNU= -O3 -march=native $(BENCH) -DKKS=$(KKS) -DKKE=$(KKE) -DGNU=0 -DQM=$(QM) -DQD=$(QD) -DIMP=$(IMP) -DITER=$(ITER) -DLD=$(LD) -DDISTR=$(DISTR) -DNUM_DIMS=$(DIMS) -D$(V) -DCOUNT_DT=$(DT) -DPROFILER=$(PROFILER) -ffast-math -funroll-loops -msse -msse2 -msse3 -msse4.1 -mbmi2 -mmmx -mavx -mavx2 -fomit-frame-pointer -m64 -fopenmp
CC_OPT_FLAGS_INTEL= -O3 -DNUM_DIMS=$(DIMS) -D$(V) -DCOUNT_DT=$(DT) -DPROFILER=$(PROFILER) -ffast-math -funroll-loops -fomit-frame-pointer -mavx -fopenmp

#GPU CONFIGURATION
GC_MAIN=gpu/main.cu
GC_EXE=gpu_run
#NVCC_FLAGS = --ptxas-options=-v -gencode arch=compute_35,code=sm_35 -rdc=true
GPU_FLAGS=-O3 #-mavx -fopenmp #--use_fast_math --gpu-architecture=compute_70 --gpu-code=sm_70
#ARCH = -gencode arch=compute_35,code=sm_35
GPU_PARAMETERS= $(GBENCH) -DKKS=$(KKS) -DKKE=$(KKE) -DGNU=0 -DQM=$(QM) -DQD=$(QD) -DIMP=$(IMP) -DITER=$(ITER) -DLD=$(LD) -DDISTR=$(DISTR) -DNUM_DIMS=$(DIMS) -DSTATS_EFF=$(STATS_EFF) -DWORKLOAD=$(WORKLOAD) -Xcompiler -fopenmp -lgomp
GPU_GCC_FLAGS=-ffast-math -funroll-loops -msse -msse2 -msse3 -msse4.1 -mbmi2 -mmmx -mavx -mavx2 -fomit-frame-pointer -m64 -fopenmp

all: cpu_cc

cpu_cc:
ifeq ($(CC),g++)
	$(CC) $(CC_FLAGS) $(CC_OPT_FLAGS_GNU) $(CC_MAIN) -o $(CC_EXE)
else
	$(CC) $(CC_FLAGS) $(CC_OPT_FLAGS_INTEL) $(CC_MAIN) -o $(CC_EXE)
endif

reorder_cc:
	$(CC) $(CC_FLAGS) $(CC_OPT_FLAGS) $(CC_REORDER) -o $(CC_EXE_RE)

gpu_cc:
	$(NVCC) -c $(NVCC_LIBS) -std=c++11 $(GPU_PARAMETERS) $(GPU_FLAGS) $(GC_MAIN) $(NVCC_INCLUDE)
	$(CC) $(CC_FLAGS) -DNUM_DIMS=$(DIMS) $(GPU_GCC_FLAGS) -o $(GC_EXE) input/randdataset-1.1.0/src/randdataset.cpp tools/tools.cpp main.o $(NVCC_LIBS) -lcudart
	
clean:
	rm -rf $(CC_EXE)
	rm -rf $(GC_EXE) 
	rm -rf $(CC_EXE_RE)
	rm -rf *.o
