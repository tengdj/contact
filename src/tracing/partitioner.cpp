/*
 * partitioner.cpp
 *
 *  Created on: Feb 1, 2021
 *      Author: teng
 */

#include "trace.h"


//void partitioner::pack_grids(query_context &ctx){
//	struct timeval start = get_cur_time();
//	size_t total_objects = 0;
//	for(vector<Point *> &ps:grids){
//		total_objects += ps.size();
//	}
//	Point *data = (Point *)new double[2*total_objects];
//	uint *offset_size = new uint[grids.size()*2];
//	int *result = new int[grids.size()];
//	uint cur_offset = 0;
//	size_t num_objects = 0;
//	size_t calculation = 0;
//	int max_one = 0;
//	int have_count = 0;
//	for(int i=0;i<grids.size();i++){
//		offset_size[i*2] = cur_offset;
//		offset_size[i*2+1] = grids[i].size();
//		for(int j=0;j<grids[i].size();j++){
//			data[cur_offset++] = *grids[i][j];
//		}
//		num_objects += grids[i].size();
//		calculation += grids[i].size()*grids[i].size();
//		if(grids[i].size()>grids[max_one].size()){
//			max_one = i;
//		}
//		have_count += grids[i].size()>0;
//	}
//	//cout<<max_one<<" "<<grids[max_one].size()<<" "<<have_count<<endl;
//	calculation /= 2;
//	//pack the grids into array
//	ctx.target[0] = (void *)data;
//	ctx.target[1] = (void *)offset_size;
//	ctx.target[2] = (void *)result;
//	ctx.num_objects = grids.size();
//	logt("packed into %ld grids %ld objects %ld calculations",start,grids.size(),num_objects, calculation);
//}
//
//
void *grid_partition(void *arg){
	query_context *ctx = (query_context *)arg;
	Grid *grid = (Grid *)ctx->target[0];
	Point *points = (Point *)ctx->target[1];
	vector<vector<uint>> *grids = (vector<vector<uint>> *)ctx->target[2];

	double x_buffer = ctx->config.reach_distance/1000*degree_per_kilometer_longitude(grid->space.low[1]);
	double y_buffer = ctx->config.reach_distance/1000*degree_per_kilometer_latitude;
	vector<size_t> gids;
	size_t start = 0;
	size_t end = 0;
	size_t num_objects = 0;
	// pick one object for generating
	while(ctx->next_batch(start,end)){
		for(int pid=start;pid<end;pid++){
			Point *p = points+pid;
			size_t gid = grid->getgrids(p,x_buffer,y_buffer);
			gids.push_back(gid);
		}
		ctx->lock();
		for(uint pid=start;pid<end;pid++){
			size_t gid_code = gids[pid-start];
			size_t gid = gid_code>>4;
			bool top_right = gid_code & 8;
			bool right = gid_code & 4;
			bool bottom_right = gid_code &2;
			bool top = gid_code &1;
			(*grids)[gid].push_back(pid);
			if(top_right){
				(*grids)[gid+grid->dimx+1].push_back(pid);
			}
			if(right){
				(*grids)[gid+1].push_back(pid);
			}
			if(top){
				(*grids)[gid+grid->dimx].push_back(pid);
			}
			if(bottom_right){
				(*grids)[gid-grid->dimx+1].push_back(pid);
			}
		}
		ctx->unlock();
		gids.clear();
	}
	return NULL;
}
//
query_context grid_partitioner::partition(Point *points, size_t num_objects){
	struct timeval start = get_cur_time();
	clear();

	vector<vector<uint>> grids;
	grids.resize(grid->get_grid_num()+1);
	query_context tctx;

	tctx.target[0] = (void *)grid;
	tctx.target[1] = (void *)points;
	tctx.target[2] = (void *)&grids;

	tctx.num_objects = num_objects;
	tctx.report_gap = 10;
	tctx.batch_size = 1;
	pthread_t threads[config.num_threads];
	for(int i=0;i<config.num_threads;i++){
		pthread_create(&threads[i], NULL, grid_partition, (void *)&tctx);
	}
	for(int i = 0; i < config.num_threads; i++ ){
		void *status;
		pthread_join(threads[i], &status);
	}
	size_t total_objects = 0;
	size_t num_grids = 0;
	for(vector<uint> &gs:grids){
		if(gs.size()>0){
			total_objects += gs.size();
			num_grids++;
		}
	}
	logt("space is partitioned into %ld grids %ld objects",start,num_grids,total_objects);


	uint *data = new uint[total_objects];
	offset_size *os = new offset_size[num_grids];
	uint *result = new uint[num_grids];

	// pack the data
	int curnode = 0;
	for(vector<uint> &gs:grids){
		if(gs.size()>0){
			if(curnode==0){
				os[curnode].offset = 0;
			}else{
				os[curnode].offset = os[curnode-1].offset+os[curnode-1].size;
			}
			os[curnode].size = gs.size();
			uint *cur_data = data + os[curnode].offset;
			for(int i=0;i<gs.size();i++){
				cur_data[i] = gs[i];
			}
			curnode++;
		}
	}

	//pack the grids into array
	query_context ctx;
	ctx.config = config;
	ctx.target[0] = (void *)points;
	ctx.target[1] = (void *)data;
	ctx.target[2] = (void *)os;
	ctx.target[3] = (void *)result;
	ctx.num_objects = num_grids;

	logt("packed into %ld grids %ld objects",start,num_grids,total_objects);
	return ctx;
}

//void *split_qtree_unit(void *arg){
//	query_context *ctx = (query_context *)arg;
//	queue<QTNode *> *qq = (queue<QTNode *> *)ctx->target[0];
//	while(!qq->empty()||!ctx->all_idle()){
//		QTNode *node = NULL;
//		ctx->lock();
//		if(!qq->empty()){
//			node = qq->front();
//			qq->pop();
//		}
//		ctx->unlock();
//		if(node){
//			ctx->busy();
//			if(node->split()){
//				ctx->lock();
//				qq->push(node->children[0]);
//				qq->push(node->children[1]);
//				qq->push(node->children[2]);
//				qq->push(node->children[3]);
//				ctx->unlock();
//			}
//			ctx->idle();
//		}
//
//		if(qq->empty()){
//			sleep(1);
//		}
//	}
//	return NULL;
//}
//
//void split_qtree(QTNode *qtree, Point *points, size_t num_objects, int num_threads){
//	query_context tctx;
//	queue<QTNode *> qq;
//	qq.push(qtree);
//	tctx.target[0] = (void *)&qq;
//	tctx.report_gap = 10;
//	pthread_t threads[num_threads];
//	for(int i=0;i<num_threads;i++){
//		pthread_create(&threads[i], NULL, split_qtree_unit, (void *)&tctx);
//	}
//	for(int i = 0; i < num_threads; i++ ){
//		void *status;
//		pthread_join(threads[i], &status);
//	}
//}
//
//void qtree_partitioner::partition(Point *points, size_t num_objects){
//	struct timeval start = get_cur_time();
//	clear();
//	assert(qtree==NULL);
//	qtree = new QTNode(mbr, &qconfig);
//	qtree->objects.resize(num_objects);
//	for(int i=0;i<num_objects;i++){
//		qtree->objects[i] = (points+i);
//	}
//	split_qtree(qtree, points, num_objects,config.num_threads);
//	qtree->get_leafs(grids, true);
//	logt("space is partitioned into %d grids %ld objects",start,qtree->leaf_count(),qtree->num_objects());
//	//qtree->print();
//}


void *insert_qtree_unit(void *arg){
	query_context *ctx = (query_context *)arg;
	QTNode *qtree = (QTNode *)ctx->target[0];
	Point *points = (Point *)ctx->target[1];
	size_t start = 0;
	size_t end = 0;
	// pick one object for inserting
	while(ctx->next_batch(start,end)){
		for(uint pid=start;pid<end;pid++){
			//log("%d",pid);
			qtree->insert(pid);
		}
	}
	return NULL;
}

void build_qtree(QTNode *qtree, Point *points, size_t num_objects, int num_threads){
	query_context tctx;
	tctx.target[0] = (void *)qtree;
	tctx.target[1] = (void *)points;
	tctx.num_objects = num_objects;
	tctx.report_gap = 10;
	pthread_t threads[num_threads];
	for(int i=0;i<num_threads;i++){
		pthread_create(&threads[i], NULL, insert_qtree_unit, (void *)&tctx);
	}
	for(int i = 0; i < num_threads; i++ ){
		void *status;
		pthread_join(threads[i], &status);
	}
}

query_context qtree_partitioner::partition(Point *points, size_t num_objects){
	struct timeval start = get_cur_time();
	clear();
	qtree = new QTNode(mbr, &qconfig);
	qconfig.points = points;
	build_qtree(qtree, points, num_objects,config.num_threads);
	//qtree->get_leafs(grids, true);
	size_t total_objects = qtree->num_objects();
	size_t num_grids = qtree->leaf_count();
	logt("space is partitioned into %d grids %ld objects",start,num_grids,total_objects);

	uint *data = new uint[total_objects];
	offset_size *os = new offset_size[num_grids];
	uint *result = new uint[num_grids];
	int curnode = 0;
	qtree->pack_data(data, os, curnode);

	//pack the grids into array
	query_context ctx;
	ctx.config = config;
	ctx.target[0] = (void *)points;
	ctx.target[1] = (void *)data;
	ctx.target[2] = (void *)os;
	ctx.target[3] = (void *)result;
	ctx.num_objects = num_grids;
	logt("packed into %ld grids %ld objects",start,num_grids,total_objects);

	//qtree->print();
	return ctx;
}


void qtree_partitioner::clear(){
	if(qtree){
		delete qtree;
		qtree = NULL;
	}
}

