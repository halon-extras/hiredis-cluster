all: hiredis-cluster

hiredis-cluster:
	g++ -L/usr/local/lib64/ -I/opt/halon/include/ -I/usr/local/include/hiredis_cluster/ -I/usr/local/include/ -lhiredis -lhiredis_cluster -fPIC -shared hiredis-cluster.cpp -o hiredis-cluster.so
