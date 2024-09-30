#include "mst.h"

#include <queue>          // std::priority_queue
#include <vector>         // std::vector
#include <functional>     // std::greater


edge new_edge( vertex & v1, vertex & v2, int src, int des ) 
{
    edge e;

    e.src       = src;
    e.des       = des;
    e.removed   = false;

    // 下面一段是 欧式距离，应该抽象出来，权重的计算方法有好多种
    float x = ((v1.x - v2.x) * (v1.x - v2.x));
    float y = ((v1.y - v2.y) * (v1.y - v2.y));

    e.weight = sqrt(x + y);

    return e;
}



vertex new_vertex( float x, float y ) 
{
    vertex v;

    v.group   = 0;    // 这里设置0 做什么用途？
    v.x       = x;
    v.y       = y;
    v.visited = false;

    return v;
}


graph new_graph( int vertex_num, vector<vertex> & vertices ) 
{
    graph g;

    g.vertex_num = vertex_num;
    g.vertices   = vertices;

    for (int i = 0 ; i < vertex_num ; i++) 
    {
        vector<edge> row;

        for(int j = 0 ; j < vertex_num ; j++) 
        {
            // 创建所有的边， 这里是 vertex_num * vertex_num 的次数，耗时要小心
            row.push_back(new_edge(vertices.at(i), vertices.at(j), i, j));
        }

        g.edges.push_back(row);
    }

    return g;
}


// 获取所有的边
vector<edge> get_all_edge(graph & g) 
{
    vector<edge> v;

    for (vector<edge> & row : g.edges) 
    {
        for( edge & col : row ) 
        {
            // 创建时 a->b 和 b->a 都创建了，是无向图，保留一条就行
            if (col.src < col.des) 
                v.push_back( col );
        }
    }

    return v;
}


// 把mst切分成 K个 簇
void create_group( vector<vertex> & vertices, vector<edge> & prim_mst, int cluster_num ) 
{
    if ( prim_mst.size() <= 0 ) return;

    // 大的排在前面
    auto cmp_pq = [](edge left, edge right) { return (left.weight) < (right.weight); };

    // 一个优先级队列,  从大到小排列
    priority_queue< edge, vector<edge>, decltype(cmp_pq) >  pq(cmp_pq);

    for (edge & e : prim_mst) 
        pq.push(e);

    int i = 0;
    while ( i < cluster_num - 1 ) 
    {
        // 从大到小， 逐个取出K条边，把他删除
        edge rem = pq.top();  pq.pop();

        for (unsigned int i = 0 ; i < prim_mst.size() ; i++) 
        {
            // prim_mst 里面没排序，挨个找一下，找到这个要删除的边
            if (prim_mst.at(i).src == rem.src && prim_mst.at(i).des == rem.des) 
            {
                prim_mst.at(i).removed = true; 
                break;
            }
        }

        i++;
    }

    // mst 是一个按连接次序，排列的边的数组，其中一些边因为权重大 删除了
    // 以其为边界，编号为不同的 簇
    int group = 1;
    for ( edge & e : prim_mst ) 
    {
        if (e.removed) 
        {
            group++;

            vertices.at(e.src).group = group;

            if (vertices.at(e.des).group == 0) 
                vertices.at(e.des).group = group;
        } 
        else 
        {
            vertices.at(e.des).group = vertices.at(e.src).group = group;
        }
    }
}


// prim算法：生成最小生成树
vector<edge> primsAlgorithm( graph & g ) 
{
    // 小的排在前面
    auto cmp_pq = [](edge left, edge right) { return (left.weight) > (right.weight); };

    // 把所有的边 放入一个 从小到大 排序的 优先队列
    priority_queue< edge, vector<edge>, decltype(cmp_pq) >  pq(cmp_pq);
    
    // 最小生成树    
    vector<edge>  mst;

    // 先 用个方法 找到一个起点, 把起点 visited = true
    g.vertices[0].visited = true;  // 偷懒先用0

    // 找到起点所有的边，放入 优先队列， 
    vector<edge> & edge_arr = g.edges[0];

    // 理论上此时加入的边的 src 都是 0
    for ( edge & e : edge_arr )
        pq.push( e );

    // 开始循环 优先队列
    while ( pq.size() > 0 ) 
    {
        // 取最小的边，检查 终点 visited， 如果为false, 则设置为true, 把边存下来
        edge min_edge = pq.top(); pq.pop();

        if ( true == g.vertices[ min_edge.des ].visited )
            continue;

        g.vertices[ min_edge.des ].visited = true;
        mst.push_back( min_edge );

        // 一直循环 到 取得的边 等于 顶点数-1 , 或者 边没有了
        if ( mst.size() >= g.vertex_num - 1 )
            break;

        // 同时取这个终点 对应的 边， 放入优先队列
        vector<edge> & edge_arr = g.edges[ min_edge.des ];

        for ( edge & e : edge_arr )
            pq.push( e );
    }

    return mst;
}
