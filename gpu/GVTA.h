#ifndef GVTA_H
#define GVTA_H

#define GVTA_PARTITIONS 2
#define GVTA_BLOCK_SIZE 8

template<class T,class Z>
struct gvta_block
{
	gvta_block() : data(NULL), tvector(NULL), num_tuples(0){}
	T *data;
	T *tvector;
	Z num_tuples;
};

template<class T, class Z>
class GVTA : public GAA<T,Z>{
	public:
		GVTA(uint64_t n, uint64_t d) : GAA<T,Z>(n,d){
			this->algo = "GVTA";
		};

		~GVTA()
		{
			if(blocks != NULL)
			{
				for(uint64_t i = 0; i < this->num_blocks;i++)
				{
					if( this->blocks[i].data != NULL ) cudaFreeHost(this->blocks[i].data);
					//if( this->blocks[i].tvector != NULL ) free(this->blocks[i].tvector);
				}
				cudaFreeHost(this->blocks);
			}
		};

		void alloc();
		void init();

	private:
		uint64_t tuples_per_part;
		uint64_t num_blocks;
		gvta_block<T,Z> *blocks = NULL;
		void layer_data();

};

template<class T, class Z>
void GVTA<T,Z>::alloc(){
	cutil::safeMallocHost<T,uint64_t>(&(this->cdata),sizeof(T)*this->n*this->d,"cdata alloc");// Allocate cpu data memory
	//cutil::safeMalloc<T,uint64_t>(&(this->gdata),sizeof(T)*this->n*this->d,"gdata alloc");//Allocate gpu data memory
}

template<class T, class Z>
void GVTA<T,Z>::layer_data()
{
	dim3 block(256,1,1);
	dim3 grid(1,1,1);
	Z *grvector = NULL;
	Z *grvector_out = NULL;
	Z *dkeys_in = NULL;
	Z *dkeys_out = NULL;
	T *dvalues_in = NULL;
	T *dvalues_out = NULL;

	Z *hkeys = NULL;
	T *hvalues = NULL;
	Z *hrvector = NULL;
	void     *d_temp_storage = NULL;
	size_t   temp_storage_bytes = 0;

	/////////////////////////////////////////
	//1: Allocate space for reordered blocks
	this->tuples_per_part = ((this->n - 1)/GVTA_PARTITIONS) + 1;
	this->num_blocks = ((this->tuples_per_part - 1) / GVTA_BLOCK_SIZE) + 1;
	cutil::safeMallocHost<gvta_block<T,Z>,uint64_t>(&(this->blocks),sizeof(gvta_block<T,Z>)*this->num_blocks,"alloc gvta_blocks");
	std::cout << this->n << " = p,psz(" << GVTA_PARTITIONS <<"," << this->tuples_per_part << ") - " << "b,bsz(" << this->num_blocks << "," << GVTA_BLOCK_SIZE << ")" << std::endl;
	for(uint64_t i = 0; i< this->num_blocks; i++)//TODO:safemalloc
	{
		cutil::safeMallocHost<T,uint64_t>(&(this->blocks[i].data),sizeof(T)*GVTA_PARTITIONS*GVTA_BLOCK_SIZE*this->d,"alloc gvta_block data("+std::to_string((unsigned long long)i)+")");
		cutil::safeMallocHost<T,uint64_t>(&(this->blocks[i].tvector),sizeof(T)*GVTA_PARTITIONS*this->d,"alloc gvta_block tvector("+std::to_string((unsigned long long)i)+")");
	}

	/////////////////////////////////////////////
	//2: Allocate temporary space for reordering
	hkeys = (Z*)malloc(sizeof(Z)*this->tuples_per_part);
	hvalues = (T*)malloc(sizeof(T)*this->tuples_per_part);
	hrvector = (Z*)malloc(sizeof(Z)*this->n);
	cutil::safeMallocHost<Z,uint64_t>(&(grvector),sizeof(Z)*this->tuples_per_part,"alloc rvector");
	cutil::safeMallocHost<Z,uint64_t>(&(grvector_out),sizeof(Z)*this->tuples_per_part,"alloc rvector");
	cutil::safeMallocHost<Z,uint64_t>(&(dkeys_in),sizeof(Z)*this->tuples_per_part,"alloc dkeys_in");
	cutil::safeMallocHost<Z,uint64_t>(&(dkeys_out),sizeof(Z)*this->tuples_per_part,"alloc dkeys_out");
	cutil::safeMallocHost<T,uint64_t>(&(dvalues_in),sizeof(T)*this->tuples_per_part,"alloc dvalues_in");
	cutil::safeMallocHost<T,uint64_t>(&(dvalues_out),sizeof(T)*this->tuples_per_part,"alloc dvalues_out");
	cub::DeviceRadixSort::SortPairsDescending(d_temp_storage, temp_storage_bytes, dkeys_in, dkeys_out, dvalues_in, dvalues_out, this->tuples_per_part);
	cutil::cudaCheckErr(cudaMalloc(&d_temp_storage, temp_storage_bytes),"alloc d_temp_storage");

	uint64_t offset = 0;
	for(uint64_t i = 0; i < GVTA_PARTITIONS; i++){//a: for each partition
		//std::cout << "PART (" << i << ")" << std::endl;
		//initialize sort indices
		uint64_t psize = offset + this->tuples_per_part < this->n ? this->tuples_per_part : (this->n - offset);//b:Find partition size
		grid.x = (this->tuples_per_part-1)/block.x + 1;
		init_rvlocal<Z><<<grid,block>>>(dkeys_in,psize);//c: initialize local for sorting attributes
		cutil::cudaCheckErr(cudaDeviceSynchronize(),"Error executing init_rvlocal");

		//initialize first seen position//
		init_rvglobal<Z><<<grid,block>>>(grvector,psize);//d: initialize local vector for first seen position
		cutil::cudaCheckErr(cudaDeviceSynchronize(),"Error executing init_rvglobal");

		for(uint64_t m = 0; m < this->d; m++){
			//e: copy attributes and sort objects per attribute
			cutil::safeCopyToDevice<T,uint64_t>(dvalues_in,&this->cdata[m*this->n + offset],sizeof(T)*psize, " copy from cdata to dvalues_in");
			cub::DeviceRadixSort::SortPairsDescending(d_temp_storage, temp_storage_bytes, dvalues_in, dvalues_out, dkeys_in, dkeys_out, psize);
			cutil::cudaCheckErr(cudaDeviceSynchronize(),"Error executing SortPairsDescending");

			//f: update first seen position
			max_rvglobal<Z><<<grid,block>>>(grvector, dkeys_out,this->tuples_per_part);
			cutil::cudaCheckErr(cudaDeviceSynchronize(),"Error executing max_rvglobal");

//			if(i == 0){//DEBUG//
//				std::cout << std::fixed << std::setprecision(4);
//				cutil::safeCopyToHost<Z,uint64_t>(hkeys,dkeys_out,sizeof(Z)*psize, " copy from dkeys_out to hkeys");
//				cutil::safeCopyToHost<T,uint64_t>(hvalues,dvalues_out,sizeof(T)*psize, " copy from dvalues_out to hvalues");
//				//for(int j = 0; j < 20; j++){ std::cout << hkeys[j] << " "; } std::cout << std::endl;
//				//for(int j = 0; j < 20; j++){ std::cout << hvalues[j] << " "; } std::cout << std::endl;
//				//std::cout << " --------- " << std::endl;
//			}
		}

//		if( i == 0 )//DEBUG//
//		{
//			cutil::safeCopyToHost<Z,uint64_t>(hrvector,grvector,sizeof(Z)*psize, " copy from grvector to hrvector");
//			for(int j = 0; j < 32; j++){ std::cout << std::setfill('0') << std::setw(3) << hrvector[j] << " "; } std::cout << std::endl;
//			for(int j = 0; j < 32; j++){ std::cout << std::setfill('0') << std::setw(3) << j << " "; } std::cout << std::endl;
//			std::cout << " <---------> " << std::endl;
//		}

		////////////////////////////////////////////////////
		//Find reordered position from first seen position//
		//initialize indices for reordering//
		//g: initialize auxiliary vectors to decide position between same first seen position objects
		init_rvlocal<Z><<<grid,block>>>(dkeys_in,psize);
		cutil::cudaCheckErr(cudaDeviceSynchronize(),"Error executing init_rvlocal for final reordering");
		cub::DeviceRadixSort::SortPairs(d_temp_storage, temp_storage_bytes, grvector, grvector_out, dkeys_in, dkeys_out, psize);
		cutil::cudaCheckErr(cudaDeviceSynchronize(),"Error executing SortPairsDescending for final reordering");
//		if( i < 1 )//DEBUG//
//		{
//			cutil::safeCopyToHost<Z,uint64_t>(hrvector,grvector_out,sizeof(Z)*psize, " copy from grvector_out to hrvector for final reordering");
//			cutil::safeCopyToHost<Z,uint64_t>(hkeys,dkeys_out,sizeof(Z)*psize, " copy from dkeys_out to hkeys for final reordering");
//			for(int j = 0; j < 32; j++){ std::cout << std::setfill('0') << std::setw(3) << hrvector[j] << " "; } std::cout << std::endl;
//			for(int j = 0; j < 32; j++){ std::cout << std::setfill('0') << std::setw(3) << hkeys[j] << " "; } std::cout << std::endl;
//			std::cout << " <---------> " << std::endl;
//		}

		//h: final data rearrangement using hkeys
		cutil::safeCopyToHost<Z,uint64_t>(hkeys,dkeys_out,sizeof(Z)*psize, " copy from dkeys_out to hkeys for final reordering");
		std::cout << std::fixed << std::setprecision(4);
		int count = 0;
		for(uint64_t j = 0; j < psize; j++)//DEBUG//
		{
			uint64_t id = offset + hkeys[j];
			T mx = 0;
			for(uint64_t m = 0; m < this->d; m++){
				if( i == 1 )//DEBUG//
				{
					mx = std::max(mx, this->cdata[this->n * m+ id]);
					std::cout << this->cdata[this->n * m+ id] << " ";
				}
			}

			if(i == 1)//DEBUG//
			{
				std::cout << "(" << mx << ")" << std::endl;
				if((j+1) % this->d == 0) std::cout << "-------" << std::endl;
				if((j+1) % GVTA_BLOCK_SIZE == 0){
					count ++;
					std::cout << "_________________________________________________" << std::endl;
					if(count == 2) break;
				}
			}
		}

		//h: final data rearrangement using hkeys
		cutil::safeCopyToHost<Z,uint64_t>(hkeys,dkeys_out,sizeof(Z)*psize, " copy from dkeys_out to hkeys for final reordering");
		uint64_t bi = 0;
		T tvector[NUM_DIMS];
		for(uint64_t j = 0; j < this->tuples_per_part; j+=GVTA_BLOCK_SIZE)
		{
			for(uint64_t m = 0; m < this->d; m++) tvector[m] = 0;
			//for(uint64_t jj = j; jj < j + GVTA_BLOCK_SIZE; jj++)
			for(uint64_t jj = 0; jj < GVTA_BLOCK_SIZE; jj++)
			{
				if(j + jj < psize){//
					uint64_t id = offset + hkeys[j+jj];
					for(uint64_t m = 0; m < this->d; m++)
					{
						T v = this->cdata[this->n * m + id];
						this->blocks[bi].data[GVTA_BLOCK_SIZE * GVTA_PARTITIONS * m + (i *GVTA_BLOCK_SIZE + jj)] = v;
						tvector[m] = std::max(tvector[m],v);
					}
				}else{//initialize to negative if block is partially full
					for(uint64_t m = 0; m < this->d; m++)
					{
						this->blocks[bi].data[GVTA_BLOCK_SIZE * GVTA_PARTITIONS * m + (i *GVTA_BLOCK_SIZE + jj)] = -1.0;
					}
				}
			}
			if(bi > 0){ std::memcpy(&this->blocks[bi-1].tvector[this->d * i], tvector,sizeof(T)*this->d); }
			bi++;
		}
		//
		offset += this->tuples_per_part;
	}

	//DEBUG//
	std::cout << std::fixed << std::setprecision(4);
	for(uint64_t b = 0; b < 4; b++){
		T *data = this->blocks[b].data;
		T *tvector = this->blocks[b].tvector;

		for(uint64_t m = 0; m < this->d; m++)
		{
			uint64_t poff = 0;
			for(uint64_t j = 0; j < GVTA_BLOCK_SIZE * GVTA_PARTITIONS; j++)
			{
				std::cout << data[GVTA_BLOCK_SIZE * GVTA_PARTITIONS * m + j] << " ";
				if((j+1) % GVTA_BLOCK_SIZE == 0){
					//std::cout << "(" << tvector[ (j % GVTA_BLOCK_SIZE)   +  (j / (GVTA_BLOCK_SIZE * GVTA_PARTITIONS))] <<")";
					std::cout << "(" << tvector[poff + m] <<")";
					//std::cout << "(0)";
					std::cout << " | ";
					poff+=this->d;
				}
			}
			std::cout << std::endl;
		}
		std::cout << "____________________________________________________________________________________________________________________________\n";
	}

	/////////////////////////
	//Free not needed space//
	cudaFree(d_temp_storage);
	cudaFree(grvector);
	cudaFree(grvector_out);
	cudaFree(dkeys_in);
	cudaFree(dkeys_out);
	cudaFree(dvalues_in);
	cudaFree(dvalues_out);

	free(hkeys);
	free(hvalues);
	free(hrvector);
}

template<class T, class Z>
void GVTA<T,Z>::init()
{
	this->layer_data();
}

#endif
