/*
 * lookup.cpp
 *
 *  Created on: Feb 25, 2021
 *      Author: teng
 */

#include "workbench.h"


//
///*
// *
// * the CPU functions for looking up QTree with points
// *
// * */
//void lookup_rec(QTSchema *schema, Point *p, uint curnode, vector<uint> &nodes, double max_dist, bool query_all){
//
//	// could be possibly in multiple children with buffers enabled
//	for(int i=0;i<4;i++){
//		uint child_offset = schema[curnode].children[i];
//		double dist = schema[child_offset].mbr.distance(*p, true);
//		if(dist<=max_dist){
//			if(schema[child_offset].type==LEAF){
//				// the node is on the top or right
//				if(query_all||
//				   p->y<schema[child_offset].mbr.low[1]||
//				   (p->y<schema[child_offset].mbr.high[1]
//				    && p->x<schema[child_offset].mbr.low[0])){
//					//schema[child_offset].mbr.print();
//					nodes.push_back(child_offset);
//				}
//			}else{
//				lookup_rec(schema, p, child_offset, nodes, max_dist, query_all);
//			}
//		}
//	}
//}
//
//// single thread function for looking up the schema to generate point-grid pairs for processing
//void *lookup_unit(void *arg){
//	query_context *qctx = (query_context *)arg;
//	workbench *bench = (workbench *)qctx->target[0];
//
//	// pick one point for looking up
//	size_t start = 0;
//	size_t end = 0;
//	vector<uint> nodes;
//	checking_unit *cubuffer = new checking_unit[2000];
//	uint buffer_index = 0;
//	while(qctx->next_batch(start,end)){
//		for(uint sid=start;sid<end;sid++){
//			uint pid = bench->global_stack[0][2*sid];
//			uint curnode = bench->global_stack[0][2*sid+1];
//
//			Point *p = bench->points+pid;
//			lookup_rec(bench->schema, p, curnode, nodes, qctx->config->reach_distance);
//			for(uint n:nodes){
//				uint gid = bench->schema[n].grid_id;
//				cubuffer[buffer_index].pid = pid;
//				cubuffer[buffer_index].gid = gid;
//				cubuffer[buffer_index].inside = false;
//				buffer_index++;
//				if(buffer_index==2000){
//					bench->batch_check(cubuffer, buffer_index);
//					buffer_index = 0;
//				}
//			}
//			nodes.clear();
//		}
//	}
//	bench->batch_check(cubuffer, buffer_index);
//	delete []cubuffer;
//	return NULL;
//}
//
//void workbench::lookup(){
//	struct timeval start = get_cur_time();
//	// partitioning current batch of objects with the existing schema
//	pthread_t threads[config->num_threads];
//	query_context qctx;
//	qctx.config = config;
//	qctx.num_units = global_stack_index[0];
//	qctx.target[0] = (void *)this;
//
//	// tree lookups
//	for(int i=0;i<config->num_threads;i++){
//		pthread_create(&threads[i], NULL, lookup_unit, (void *)&qctx);
//	}
//	for(int i = 0; i < config->num_threads; i++ ){
//		void *status;
//		pthread_join(threads[i], &status);
//	}
//	logt("lookup: %d pid-gid pairs need be checked",start,grid_check_counter);
//}
