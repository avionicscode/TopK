#include "tools.h"
#include <parallel/algorithm>

template<class T, class Z>
void psort(gpta_pair<T,Z> *tpairs,uint64_t n, bool ascending){
	if(ascending){
		__gnu_parallel::sort(&tpairs[0],(&tpairs[0])+n,cmp_gpta_pair_asc<T,Z>);
	}else{
		__gnu_parallel::sort(&tpairs[0],(&tpairs[0])+n,cmp_gpta_pair_desc<T,Z>);
	}
}

template void psort(gpta_pair<float,uint64_t> *tpairs,uint64_t n, bool ascending);
template void psort(gpta_pair<float,uint32_t> *tpairs,uint64_t n, bool ascending);

template<class Z>
void ppsort(gpta_pos<Z> *tpos, uint64_t n){
	__gnu_parallel::sort(&tpos[0],(&tpos[0])+n,cmp_gpta_pos_asc<Z>);
}

template void ppsort(gpta_pos<uint64_t> *tpos,uint64_t n);
template void ppsort(gpta_pos<uint32_t> *tpos,uint64_t n);


template<class T, class Z>
void pnth_element(gpta_pair<T,Z> *tscore, uint64_t n, uint64_t k, bool ascending){
	__gnu_parallel::nth_element(&tscore[0],(&tscore[k]),(&tscore[0])+n,cmp_gpta_pair_asc<T,Z>);
}

template void pnth_element(gpta_pair<float,uint32_t> *tscore, uint64_t n, uint64_t k, bool ascending);
template void pnth_element(gpta_pair<float,uint64_t> *tscore, uint64_t n, uint64_t k, bool ascending);

template <class T>
void normalize_transpose(T *&cdata, uint64_t n, uint64_t d){
	T *mmax = static_cast<T*>(aligned_alloc(32,sizeof(T)*d));
	T *mmin = static_cast<T*>(aligned_alloc(32,sizeof(T)*d));

	//Find min and max for each attribute list
	for(uint64_t m = 0; m < d; m++){
		mmax[m] = 0;
		mmin[m] = std::numeric_limits<T>::max();
		for(uint64_t i = 0; i < n; i++){
			T value = cdata[m*n + i];
			//if (m == 0) std::cout << m << " < " << value << "," << value <<"," << mmax[m] << std::endl;
			mmax[m] = std::max(mmax[m],value);
			mmin[m] = std::min(mmin[m],value);
		}
	}

	//Normalize values
	for(uint64_t m = 0; m < d; m++){
		T _max = mmax[m];
		T _min = mmin[m];
		T _mm = _max - _min;
		//if ( _mm == 0 ){ std::cout << m << " <"<< _max << " - " << _min << std::endl; }
		for(uint64_t i = 0; i < n; i++){
			T value = cdata[m*n+i];
			value = (value - _min)/_mm;
			cdata[m*n + i] = value;
		}
	}
	free(mmax);
	free(mmin);
}

template void normalize_transpose(float *&cdata, uint64_t n, uint64_t d);
