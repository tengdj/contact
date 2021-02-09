/*
 * generator.cpp
 *
 *  Created on: Jan 19, 2021
 *      Author: teng
 */

#include "trace.h"

#include "../index/QTree.h"






/*
 * functions for tracer
 *
 * */

void *process_grid_unit(void *arg){
	query_context *ctx = (query_context *)arg;
	Point *points = (Point *)ctx->target[0];
	uint *pids = (uint *)ctx->target[1];
	offset_size *os = (offset_size *)ctx->target[2];
	uint *result = (uint *)ctx->target[3];
	uint *gridassign = (uint *)ctx->target[4];
	size_t checked = 0;
	size_t reached = 0;
	int max_len = 0;
	while(true){
		// pick one object for generating
		size_t start = 0;
		size_t end = 0;
		if(!ctx->next_batch(start,end)){
			break;
		}
		for(int pid=start;pid<end;pid++){
			uint gid = gridassign[pid];
			uint len = os[gid].size;
			uint *cur_pids = pids + os[gid].offset;
			result[pid] = 0;
			//vector<Point *> pts;
			if(len>2){
				Point *p1 = points+pid;
				for(uint i=0;i<os[gid].size;i++){
					//pts.push_back(points + cur_pids[i]);
					Point *p2 = points + cur_pids[i];
					if(p1!=p2){
						//log("%f",dist);
						result[pid] += p1->distance(*p2, true)<ctx->config.reach_distance;
						checked++;
					}
				}
				reached += result[gid];
			}
		}

	}
	lock();
	ctx->checked += checked;
	ctx->found += reached;
	unlock();
	return NULL;
}

void process_grids(query_context &tctx){
	struct timeval start = get_cur_time();
	pthread_t threads[tctx.config.num_threads];
	tctx.clear();

	for(int i=0;i<tctx.config.num_threads;i++){
		pthread_create(&threads[i], NULL, process_grid_unit, (void *)&tctx);
	}
	for(int i = 0; i < tctx.config.num_threads; i++ ){
		void *status;
		pthread_join(threads[i], &status);
	}
	logt("compute",start);
}


void process_with_gpu(query_context &ctx);

void tracer::process(){

	struct timeval start = get_cur_time();
	// test contact tracing
	size_t checked = 0;
	size_t reached = 0;
	for(int t=0;t<config.duration;t++){

		// first level of partition
		uint *pids = new uint[config.num_objects];
		for(int i=0;i<config.num_objects;i++){
			pids[i] = i;
		}
		Point *cur_trace = trace+t*config.num_objects;

		print_points(cur_trace,config.num_objects,(10000.0/config.num_objects));
		return;
		query_context tctx = part->partition(cur_trace, pids, config.num_objects);
		// process the objects in the packed partitions
		if(!config.gpu){
			process_grids(tctx);
		}else{
			process_with_gpu(tctx);
		}
		checked += tctx.checked;
		reached += tctx.found;


		/*
		 *
		 * some statistics printing for debuging only
		 *
		 * */
//		map<int, uint> connected;
//
//		uint *data = (uint *)tctx.target[1];
//		offset_size *os = (offset_size *)tctx.target[2];
//		uint *result = (uint *)tctx.target[3];
//		uint *gridassign = (uint *)tctx.target[4];
//
//		uint max_one = 0;
//		for(int i=0;i<config.num_objects;i++){
//			if(connected.find(result[i])==connected.end()){
//				connected[result[i]] = 1;
//			}else{
//				connected[result[i]]++;
//			}
//			if(result[max_one]<result[i]){
//				max_one = i;
//			}
//		}
//
//		for(auto a:connected){
//			cout<<a.first<<" "<<a.second<<endl;
//		}

//		cout<<max_one<<" "<<result[max_one]<<endl;
//		uint gid = gridassign[max_one];
//		uint *cur_pid = data + os[gid].offset;
//		vector<Point *> all_points;
//		vector<Point *> valid_points;
//		Point *p1 = cur_trace + max_one;
//		for(uint i=0;i<os[gid].size;i++){
//			Point *p2 = cur_trace+cur_pid[i];
//			if(p1==p2){
//				continue;
//			}
//			all_points.push_back(p2);
//			double dist = p1->distance(*p2,true);
//			if(i<10){
//				cout<<dist<<endl;
//				p1->print();
//				p2->print();
//			}
//
//			if(dist<config.reach_distance){
//				valid_points.push_back(p2);
//			}
//		}
//		print_points(all_points);
//		print_points(valid_points);
//		p1->print();
//		cout<<all_points.size()<<" "<<valid_points.size()<<endl;
	}

	logt("contact trace with %ld calculation %ld connected",start,checked,reached);
}

void tracer::dumpTo(const char *path) {
	struct timeval start_time = get_cur_time();
	ofstream wf(path, ios::out|ios::binary|ios::trunc);
	wf.write((char *)&config.num_objects, sizeof(config.num_objects));
	wf.write((char *)&config.duration, sizeof(config.duration));
	wf.write((char *)&mbr, sizeof(mbr));
	size_t num_points = config.duration*config.num_objects;
	wf.write((char *)trace, sizeof(Point)*num_points);
	wf.close();
	logt("dumped to %s",start_time,path);
}

void tracer::loadFrom(const char *path) {

	int total_num_objects;
	int total_duration;
	struct timeval start_time = get_cur_time();
	ifstream in(path, ios::in | ios::binary);
	in.read((char *)&total_num_objects, sizeof(total_num_objects));
	in.read((char *)&total_duration, sizeof(total_duration));
	in.read((char *)&mbr, sizeof(mbr));
	mbr.to_squre(true);
	assert(config.duration*config.num_objects<=total_duration*total_num_objects);

	trace = (Point *)malloc(config.duration*config.num_objects*sizeof(Point));
	for(int i=0;i<config.duration;i++){
		in.read((char *)(trace+i*config.num_objects), config.num_objects*sizeof(Point));
		if(total_num_objects>config.num_objects){
			in.seekg((total_num_objects-config.num_objects)*sizeof(Point), ios_base::cur);
		}
	}
	in.close();
	logt("loaded %d objects last for %d seconds from %s",start_time, config.num_objects, config.duration, path);
	owned_trace = true;
}

void tracer::print_trace(double sample_rate){
	print_points(trace,config.num_objects,sample_rate);
}


