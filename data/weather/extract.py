import sys

def extract_csv(fname):
    station = dict()
    
    print "Loading file ... "
    with open(fname,'r') as fp:
        for line in fp:
            data = line.strip().split(",")
            
            if data[2] == "TMAX" or data[2] == "TMIN":
                sid = data[0]
                #print sid
                if sid not in station:
                    station[sid] = dict()
                
                type = data[2]
                if type not in station[sid]:
                    station[sid][type] = dict()
                
                date = data[1][4:]
                if date not in station[sid][type]:
                    station[sid][type][date] = list()
                station[sid][type][date].append(float(data[3])/10)
                    
    print "Finish loading file ... "
    return station

def view_data(station):
    global attributes
    
    count = 0
    for s in station:
        for t in station[s]:
            for d in station[s][t]:
                if len(station[s][t][d]) >= attributes:
                    #print s, t, d,
                    #for v in station[s][t][d]:
                        #print v,
                    #print ""
                    count+=1
    
    print "tuples: ", count

def write_csv(fname,station):
    global attributes
    fp = open(fname.split(".")[0],'w')
    count = 0
    for s in station:
        for t in station[s]:
            for d in station[s][t]:
                if len(station[s][t][d]) >= attributes:
                    size = len(station[s][t][d])
                    for v in station[s][t][d]:
                        fp.write(str(v)+",")
                    count+=1
    fp.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "Usage:",sys.argv[0],"<file> <attributes>"
        exit(1)
    
    fname = sys.argv[1]
    attributes = int(sys.argv[2])
    print "Process file: ", fname, attributes
    station=extract_csv(fname)
    view_data(station)
