#include "time/Time.h"
#include "tools/ArgParser.h"
#include "input/File.h"

#include "cpu/AA.h"
#include "cpu/NA.h"
#include "cpu/FA.h"
#include "cpu/TA.h"
#include "cpu/CBA.h"

void debug(std::string fname, uint64_t k){
	File<float> f(fname,false);
	File<float> f2(fname,false,f.rows(),f.items());
	f2.set_transpose(true);

	NA<float> na(f.rows(),f.items());
	FA<float> fa(f.rows(),f.items());
	TA<float> ta(f.rows(),f.items());
	CBA<float> cba(f.rows(),f.items());

	f.load(na.get_cdata());
	f2.load(cba.get_cdata());
	fa.set_cdata(na.get_cdata());
	ta.set_cdata(na.get_cdata());

//	cba.set_cdata(na.get_cdata());
//	cba.transpose();

	na.init(); na.findTopK(k);
	fa.init(); fa.findTopK(k);
	ta.init(); ta.findTopK(k);
	cba.init(); cba.findTopK(k);

	fa.compare(na);
	ta.compare(na);
	cba.compare(na);

	na.benchmark();
	fa.benchmark();
	ta.benchmark();
	cba.benchmark();
}

int main(int argc, char **argv){
	ArgParser ap;
	ap.parseArgs(argc,argv);
	if(!ap.exists("-f")){
		std::cout << "Missing file input!!!" << std::endl;
		exit(1);
	}

	if(!ap.exists("-md")){
		//std::cout << "Default mode: <debug>" <<std::endl;
		ap.addArg("-md","debug");
	}

	if(ap.getString("-md") == "debug"){
		debug(ap.getString("-f"),100);
	}

	return 0;
}
