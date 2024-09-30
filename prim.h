#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <functional>
#include <queue>

using namespace std;

// 顶点
typedef struct vertex 
{
    int   group;                        // 这个做啥用？ 难道后后面 切分mst的时候 作为 簇的标号 ？
    float x;                            // 这里的x、y应该就是 表示一个顶点的向量 
    float y;
    bool  visited;                      // 这个是 prim算法 要的，标记顶点已经探测过
} vertex;

// 边
typedef struct edge 
{
    int   src;                          // 起始的顶点，在图的顶点数组中的 下标 位置
    int   des;                          // 结束的顶点，在图的顶点数组中的 下标 位置
    float weight;                       // 权重     
    bool  removed;                      // 这个做啥用？
} edge;


// 图，无向加权图
typedef struct graph 
{
    int                  vertex_num;    // 顶点数量
    vector<vertex>       vertices;      // 顶点数组

    // 每一个顶点对应的 边的数组， 里面的位置都是按照 下标 定好的，不能随便动
    vector< vector< edge > > edges;         
} graph;


vertex new_vertex( float x, float y );
edge   new_edge( vertex & v1, vertex & v2, int src, int des );
graph  new_graph( int vertex_num, vector<vertex> & vertices );

vector<edge> get_all_edge( graph & g );

// 生成链接图的最小生成树 算法，返回所有保留的边
vector<edge> primsAlgorithm( graph & g );

// 把树按照 链接边的长度，切成 k 个簇
// 这里应该还可以 根据 边的距离长度，自动的确定 是否需要拆分，这样能自动决定 簇的个数
void   create_group( vector<vertex> & vertices, vector<edge> & prim_mst, int cluster_num );
