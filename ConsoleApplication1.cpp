#include <stdlib.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include<cstdio>
//#include "Chromosome.cpp"
//#include "Topo.h"
//#include "Chromosome.h"
#include <vector>

using namespace std;

const int slot_num = 12;    //每个调度周期的时隙数

//流信息
struct Stream
{
    int src;    //源节点
    int dst;    //目的节点
    int cycle;  //周期
    int bound;  //跳数限制
};

//节点信息
struct Node {
    int slot[slot_num];
};

const int stream_num = 40;  //流数
const int node_num = 24;    //网络节点数 24 14
int m[node_num][node_num];   //拓扑
int m_lb[node_num][node_num];  //用于lb
Stream stream[stream_num];   //流集合
Stream frstream[stream_num];    //固定路由的流集合
const int population_size = 320;    //种群规模
double rate_crossover = 0.2; //交叉率
double rate_mutation = 0.8;   //变异率
int iteration_num = 1000; //进化50代
//遗传 最短路 负载平衡 固定路由
//染色体
typedef struct Chromosome
{
    bool isset[stream_num]; //是否部署流
    int offset[stream_num]; //流的Offset属性
    int path[stream_num][node_num]; //流的路径
    int portslot[node_num][node_num][slot_num]; //端口的时隙
    int fitness;    //适应值
    double rate_fit;    //相对的fit值，即百分比
    double cumu_fit;    //累计概率
}chromosome;
//函数：计算适应值
int objFunc(const chromosome& ch);
//函数：更新中群内个体的属性值
void freshPro(chromosome* population_curr);
//函数：种群初始化
void populationInit(chromosome* population_curr, Stream* stream);
//函数：产生随机路由
void randomRouting(bool& found, const int& dst, vector<int>& path, vector<vector<bool>>& visited, vector<int>& ans);
//函数：基于轮盘赌选择方法，对种群中的染色体进行选择
void shortestRouting(bool& found, const int& dst, vector<int>& path, vector<int>& ans);
void loadBalance(bool& found, const int& dst, vector<int>& path, vector<int>& ans);
void selectPrw(chromosome* population_curr, chromosome* population_next, chromosome& best,double);
void selectPrw2(chromosome* population_curr, chromosome* population_next, chromosome& best);
// 函数：交叉操作
void crossover(chromosome* population_next);
// 函数：变异操作
void mutation(chromosome* population_next);
//初始化全局时隙
/*void switchInit(){
    for(int i=0;i<node_num;i++){
        for(int j=0;j<slot_num;j++){
            node[i].slot[j] = -1;
        }
    }
}*/
void showSlot(chromosome* ch)
{

    //输出时隙
    for (int i = 0; i < node_num; i++)
    {
        for (int j = 0; j < node_num; j++)
        {
            if (m[i][j])
            {
                cout << "node " << i << "->" << j << " slot:";
                for (int k = 0; k < slot_num; k++)
                    cout << ch->portslot[i][j][k] << " ";
            }
        }
        cout << endl;
    }
}
int objFunc(const chromosome& ch)
{

    //函数：计算适应值
    int set = 0;
    for (int i = 0; i < stream_num; i++)
    {
        if (ch.isset[i])
            ++set;
    }
    return set;
}
void freshPro(chromosome* population_curr)
{

    //函数：更新中群内个体的属性值
    //输入：chromosome& population_curr[population_size] 当代种群的引用
    //cout<<"enter freshpro"<<endl;
    int sum = 0;
    //更新每个个体的fitness
    for (int i = 0; i < population_size; i++)
    {
        population_curr[i].fitness = objFunc(population_curr[i]);
        sum += population_curr[i].fitness;
    }
    //计算每个个体的适应值百分比和累计概率，在轮盘赌算法时用它选择染色体
    population_curr[0].rate_fit = (double)population_curr[0].fitness / (double)sum;
    population_curr[0].cumu_fit = (double)population_curr[0].fitness / (double)sum;
    for (int i = 1; i < population_size; i++)
    {
        population_curr[i].rate_fit = (double)population_curr[i].fitness / (double)sum;
        population_curr[i].cumu_fit = population_curr[i].rate_fit + population_curr[i - 1].cumu_fit;
    }
}
void setOne(chromosome& ch, int index, vector<int>& route, int os)
{

    //函数：给一个种群和某条流，检查是否能放置
    //输入：
    //chromosome& ch:个体的引用
    //const int index:流的序号
    //vector<int>& route:流的路径信息
    //int os:流的offset信息
    //cout<<"enter setOne"<<endl;
    int delaysum=0;
    bool conflict = false;
    const int stream_slot_num = slot_num / stream[index].cycle;
    for (int i = 0; i < route.size() - 1; i++)
    {
        for (int j = 0; j < stream_slot_num; j++)
        {
            if (route[i] > node_num || route[i + 1] > node_num || route[i] < 0 || route[i + 1] < 0) continue;
            if (ch.portslot[route[i]][route[i + 1]][(os + i + delaysum + j * stream[index].cycle) % slot_num] >= 0)
            {
                conflict = true;
                break;
            }
        }
        delaysum += m[route[i]][route[i + 1]];
    }
    if (!conflict)
    {
        //cout<<"no conflict"<<endl;
        ch.isset[index] = true;
        ch.offset[index] = os;
        for (int i = 0; i < route.size(); i++)
        {
            ch.path[index][i] = route[i];
        }
        //cout<<"loc2"<<endl;
        delaysum = 0;
        for (int i = 0; i < route.size() - 1; i++)
        {
            for (int j = 0; j < stream_slot_num; j++)
            {
                if (route[i] > node_num || route[i + 1] > node_num || route[i] < 0 || route[i + 1] < 0) continue;
                ch.portslot[route[i]][route[i + 1]][(os + i + delaysum + j * stream[index].cycle) % slot_num] = index;
            }
            delaysum += m[route[i]][route[i + 1]];
        }
    }
    else
    {
        //cout<<"has a conflict"<<endl;
        ch.isset[index] = false;
    }
}
void populationInit(chromosome* population_curr)
{

    //函数：种群初始化
    //cout<<"enter population init"<<endl;
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        //cout<<"in population["<<i<<"]:"<<endl;
        //初始化时隙
        for (int j = 0; j < node_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                for (int m = 0; m < slot_num; m++)
                {
                    population_curr[i].portslot[j][k][m] = -1;
                }
            }
        }
        //初始化路径
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                population_curr[i].path[j][k] = -1;
            }
        }

        for (int j = 0; j < stream_num; j++)
        { //！！！流的先后顺序会有影响吗？？？？
            for (int i1 = 0; i1 < node_num; i1++)
                for (int j1 = 0; j1 < node_num; j1++) {
                    m_lb[i1][j1] = m[i1][j1];
                }
            int a = rand() % 3;
            if (a==0)
                population_curr[i].isset[j] = false;
            else
            {
                population_curr[i].offset[j] = rand() % stream[j].cycle;
                //从stream[j].src到stream[j].dst产生一条随机路由
                //并占用经过的交换机的时隙
                bool found = false;
                vector<int> path; //路径的临时变量
                path.push_back(stream[j].src);
                vector<vector<bool> > visited(node_num, vector<bool>(node_num, false));
                vector<int> ans; //计算出的随机路径
                //loadBalance(found, stream[j].dst, path, ans);
                shortestRouting(found, stream[j].dst, path, ans);
                //randomRouting(found, stream[j].dst, path, visited, ans);
                //randomRouting(found, stream[j].dst, path, visited, ans);
                //判断是否满足时延要求
                if ((rand() % 20 + (ans.size() - 1) * 10) <= stream[j].bound)
                {
                    setOne(population_curr[i], j, ans, population_curr[i].offset[j]);
                }

                else
                {
                    population_curr[0].isset[j] = false;
                }
            }
        }
    }
}
void lb_populationInit(chromosome* population_curr)
{
    //cout<<"enter frpopulation init"<<endl;
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        //初始化时隙
        for (int j = 0; j < node_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                for (int m = 0; m < slot_num; m++)
                {
                    population_curr[i].portslot[j][k][m] = -1;
                }
            }
        }
        //初始化路径
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                population_curr[i].path[j][k] = -1;
            }
        }
    }
    //初始化流路径
    for (int i = 0; i < population_size; i++)
    {
        for (int i1 = 0; i1 < node_num; i1++)
            for (int j1 = 0; j1 < node_num; j1++) {
                m_lb[i1][j1] = m[i1][j1];
            }
        for (int j = 0; j < stream_num; j++)
        {
            bool found = false;
            vector<int> path;
            path.push_back(stream[j].src);
            vector<vector<bool> > visited(node_num, vector<bool>(node_num, false));
            vector<int> ans;
            loadBalance(found, stream[j].dst, path, ans);
            //shortestRouting(found, stream[j].dst, path, ans);
            //randomRouting(found, stream[j].dst, path, visited, ans);
            for (int k = 0; k < ans.size(); k++) {
                population_curr[i].path[j][k] = ans[k];
            }
        }
    }
    for (int i = 0; i < population_size; i++)
    {
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                if (i > 0)
                {
                    //population_curr[i].path[j][k] = population_curr[0].path[j][k];
                }

            }
            int a = rand() % 2;
            population_curr[i].offset[j] = rand() % stream[j].cycle;
            if (1)//a
            {
                vector<int> path;
                for (int k = 0; k < node_num; k++)
                {
                    if (population_curr[i].path[j][k] != -1)
                    {
                        path.push_back(population_curr[i].path[j][k]);
                    }
                    else
                        break;
                }
                if ((rand() % 20 + (path.size() - 1) * 10) <= stream[j].bound)
                {
                    setOne(population_curr[i], j, path, population_curr[i].offset[j]);
                }
            }
            else
            {
                population_curr[0].isset[j] = false;
            }

        }
    }
}
void sht_populationInit(chromosome* population_curr)
{
    //cout<<"enter frpopulation init"<<endl;
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        //初始化时隙
        for (int j = 0; j < node_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                for (int m = 0; m < slot_num; m++)
                {
                    population_curr[i].portslot[j][k][m] = -1;
                }
            }
        }
        //初始化路径
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                population_curr[i].path[j][k] = -1;
            }
        }
    }
    //初始化流路径
    for (int i = 0; i < population_size; i++)
    {
        for (int j = 0; j < stream_num; j++)
        {
            bool found = false;
            vector<int> path;
            path.push_back(stream[j].src);
            vector<vector<bool> > visited(node_num, vector<bool>(node_num, false));
            vector<int> ans;
            shortestRouting(found, stream[j].dst, path, ans);
            //randomRouting(found, stream[j].dst, path, visited, ans);
            for (int k = 0; k < ans.size(); k++) {
                population_curr[i].path[j][k] = ans[k];
            }
        }
    }
    for (int i = 0; i < population_size; i++)
    {
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                if (i > 0)
                {
                    //population_curr[i].path[j][k] = population_curr[0].path[j][k];
                }

            }
            int a = rand() % 2;
            population_curr[i].offset[j] = rand() % stream[j].cycle;
            if (1)//a
            {
                vector<int> path;
                for (int k = 0; k < node_num; k++)
                {
                    if (population_curr[i].path[j][k] != -1)
                    {
                        path.push_back(population_curr[i].path[j][k]);
                    }
                    else
                        break;
                }
                if ((rand() % 20 + (path.size() - 1) * 10) <= stream[j].bound)
                {
                    setOne(population_curr[i], j, path, population_curr[i].offset[j]);
                }
            }
            else
            {
                population_curr[0].isset[j] = false;
            }

        }
    }
}
void frpopulationInit(chromosome* population_curr)
{

    //函数：固定路由的种群初始化
    //cout<<"enter frpopulation init"<<endl;
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        //初始化时隙
        for (int j = 0; j < node_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                for (int m = 0; m < slot_num; m++)
                {
                    population_curr[i].portslot[j][k][m] = -1;
                }
            }
        }
        //初始化路径
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                population_curr[i].path[j][k] = -1;
            }
        }
    }
    //初始化流路径
    for (int i = 0; i < population_size; i++)
    {
        for (int j = 0; j < stream_num; j++)
        {
            bool found = false;
            vector<int> path;
            path.push_back(stream[j].src);
            vector<vector<bool> > visited(node_num, vector<bool>(node_num, false));
            vector<int> ans;
            randomRouting(found, stream[j].dst, path, visited, ans);
            for (int k = 0; k < ans.size(); k++) {
                population_curr[i].path[j][k] = ans[k];
            }
        }
    }
    for (int i = 0; i < population_size; i++)
    {
        for (int j = 0; j < stream_num; j++)
        {
            for (int k = 0; k < node_num; k++)
            {
                if (i > 0)
                {
                    //population_curr[i].path[j][k] = population_curr[0].path[j][k];
                }

            }
            int a = rand() % 2;
            population_curr[i].offset[j] = rand() % stream[j].cycle;
            if (1)//a
            {
                vector<int> path;
                for (int k = 0; k < node_num; k++)
                {
                    if (population_curr[i].path[j][k] != -1)
                    {
                        path.push_back(population_curr[i].path[j][k]);
                    }
                    else
                        break;
                }
                if ((rand() % 20 + (path.size() - 1) * 10) <= stream[j].bound)
                {
                    setOne(population_curr[i], j, path, population_curr[i].offset[j]);
                }
            }
            else
            {
                population_curr[0].isset[j] = false;
            }

        }
    }
}
void shortestRouting(bool& found, const int& dst, vector<int>& path, vector<int>& ans) {
    vector<int> marked;
    int mark[node_num][3]; //标记大小，上一跳的节点
    for (int i = 0; i < node_num; i++) {
        mark[i][0] = 255;
        mark[i][1] = -1;
        mark[i][2] = 0;
    }
    int findmin = 255;
    int minnode=0;
    mark[path[0]][0] = 0;
    mark[path[0]][2] = 1;
    marked.push_back(path[0]);
    while (marked.size() < node_num) {
        for (int i = 0; i < marked.size(); i++) {
            int now = marked[i];
            for (int j = 0; j < node_num; j++) {
                if (mark[now][0] + m[now][j] < mark[j][0] && m[now][j]>0 && mark[j][2]==0) {
                    mark[j][0] = mark[now][0] + m[now][j];
                    mark[j][1] = now;
                }
            }
        }
        findmin = 255;
        for (int i = 0; i < node_num; i++) {
            if (mark[i][0] < findmin && mark[i][2] == 0) {
                findmin = mark[i][0];
                minnode = i;
            }
        }
        marked.push_back(minnode);
        mark[minnode][2] = 1;
        if (minnode == dst) break;
    }
    vector<int> reversepath;
    int prenode;
    reversepath.push_back(dst);
    prenode = mark[dst][1];
    if (prenode == -1) {
        found = false;
        return;
    }
    while (prenode != path[0] && prenode != -1) {
        reversepath.push_back(prenode);
        prenode = mark[prenode][1];
    }
    reversepath.push_back(prenode);
    for (int i = reversepath.size()-1; i >= 0; i--) {
        ans.push_back(reversepath[i]);
    }
    return;
}
void loadBalance(bool& found, const int& dst, vector<int>& path, vector<int>& ans) {
    vector<int> marked;
    int mark[node_num][3]; //标记大小，上一跳的节点
    for (int i = 0; i < node_num; i++) {
        mark[i][0] = 65535;
        mark[i][1] = -1;
        mark[i][2] = 0;
    }
    int findmin = 65535;
    int minnode = 0;
    mark[path[0]][0] = 0;
    mark[path[0]][2] = 1;
    marked.push_back(path[0]);
    while (marked.size() < node_num) {
        for (int i = 0; i < marked.size(); i++) {
            int now = marked[i];
            for (int j = 0; j < node_num; j++) {
                if (mark[now][0] + m_lb[now][j] < mark[j][0] && m_lb[now][j]>0 && mark[j][2] == 0) {
                    mark[j][0] = mark[now][0] + m_lb[now][j];
                    mark[j][1] = now;
                }
            }
        }
        findmin = 65535;
        for (int i = 0; i < node_num; i++) {
            if (mark[i][0] < findmin && mark[i][2] == 0) {
                findmin = mark[i][0];
                minnode = i;
            }
        }
        marked.push_back(minnode);
        mark[minnode][2] = 1;
        if (minnode == dst) break;
    }
    vector<int> reversepath;
    int prenode;
    reversepath.push_back(dst);
    prenode = mark[dst][1];
    if (prenode == -1) {
        found = false;
        return;
    }
    while (prenode != path[0] && prenode != -1) {
        reversepath.push_back(prenode);
        prenode = mark[prenode][1];
    }
    reversepath.push_back(prenode);
    for (int i = reversepath.size() - 1; i >= 0; i--) {
        ans.push_back(reversepath[i]);
    }
    for (int i = 0; i < ans.size() - 1; i++) {
        m_lb[ans[i]][ans[i + 1]] += 100;
    }
    return;
}
void randomRouting(bool& found, const int& dst, vector<int>& path, vector<vector<bool> >& visited, vector<int>& ans)
{
//函数：使用深度优搜索产生随机路由
//输入：
//bool& found：是否已经找到目的节点的二元变量
//const int& dst：目的节点
//vector<int>& path：暂存路径信息
//void** visited:路径是否已访问列表
//vector<int>& ans:返回结果
    if (path.size() > node_num)
       return;
    if (found)
    {
        //cout<<"already found"<<endl;
        return;
    }
    if (path.back() == dst)
    {
        //cout<<"found now"<<endl;
        found = true;
        ans = path;
        return;
    }
    int curr = path.back();
    vector<int> alter; //备选节点集合
    for (int i = 0; i < node_num; i++)
    {
        if (m[curr][i] != 0 && visited[curr][i] == false)
            alter.push_back(i);
    }
    if (alter.empty())
    {
        //cout<<"no way in "<<curr<<endl;
        return;
    }
    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    shuffle(alter.begin(), alter.end(), default_random_engine(seed));
    for (int i = alter.size() - 1; i >= 0; i--)
    {
        path.push_back(alter[i]);
        visited[curr][alter[i]] = true;
        visited[alter[i]][curr] = true;
        randomRouting(found, dst, path, visited, ans);
        path.pop_back();
        visited[curr][alter[i]] = false;
        visited[alter[i]][curr] = false;
    }
}
void selectPrw(chromosome* population_curr, chromosome* population_next, chromosome& best,double count)
{
    //函数：基于轮盘赌选择方法，对种群中的染色体进行选择，产生下一代种群
//输入：
//chromosome* population_curr 当代种群的数组
//chromosome* population_next 下一代种群的数组
//chromosome& best 当前代种群中的最优个体
    //cout<<"enter selectprw"<<endl;
    double rate_rand = 0;
    double rate_rand1 = 0;
    //best = population_curr[0];
    srand((unsigned)time(NULL));
    int flag = 0;
    for (int i = 0; i < population_size; i++)
    {
        //rate_rand = (float)rand() / (RAND_MAX);
        rate_rand1 = (double)(0.7+ 0.5* count/ iteration_num) / (population_size + rand() % (population_size));
        if (rate_rand1 < population_curr[i].rate_fit)
        {
            population_next[i] = population_curr[i];
            //cout<<"0"<<endl;
        }
        else //if(rate_rand1 > population_curr[i].rate_fit)
        {
            if (i == 0) {
                population_next[0] = best;
                continue;
            }
            population_next[i] = population_next[rand() % i];
            /*
            for (int j = 0; j < i; j++)
            {
                if (population_curr[j].cumu_fit <= rate_rand && rate_rand < population_curr[j + 1].cumu_fit)
                //if (population_curr[j].rate_fit >= rate_rand)
                {
                    population_next[i] = population_curr[j + 1];
                    //cout<<j+1<<endl;
                    break;
                }
                
            }*/
        }
        //如果当前个体比目前的最优个体还要优秀，则将当前个体设为最优个体
        if (population_curr[i].fitness > best.fitness)
            best = population_curr[i];
    }
}
void selectPrw2(chromosome* population_curr, chromosome* population_next, chromosome& best)
{
    //cout<<"enter selectprw"<<endl;
    double rate_rand = 0;
    best = population_curr[0];
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        rate_rand = (float)rand() / (RAND_MAX);
        if (rate_rand < population_curr[0].cumu_fit)
        {
            population_next[i] = population_curr[0];
            //cout<<"0"<<endl;
        }
        else
        {
            for (int j = 0; j < population_size; j++)
            {
                if (population_curr[j].cumu_fit <= rate_rand && rate_rand < population_curr[j + 1].cumu_fit)
                {
                    population_next[i] = population_curr[j + 1];
                    //cout<<j+1<<endl;
                    break;
                }
            }
        }
        //如果当前个体比目前的最优个体还要优秀，则将当前个体设为最优个体
        if (population_curr[i].fitness > best.fitness)
            best = population_curr[i];
    }
}
static bool cmp(const chromosome& ch1, const chromosome& ch2)
{
    return ch1.fitness < ch2.fitness;
}
void clearOne(chromosome& ch, int index, vector<int>& route)
{
    //函数：删除一个个体中流的配置
//输入：
//chromosome& ch:个体的引用
//const int index:流的序号
    ch.isset[index] = false;
    for (int j = 0; j < route.size() - 1; j++)
    {
        for (int k = 0; k < slot_num / stream[index].cycle; k++)
        {
            if (route[j] > node_num || route[j + 1] > node_num || route[j] < 0 || route[j + 1] < 0) continue;
            ch.portslot[route[j]][route[j + 1]][(ch.offset[index] + j + k * stream[index].cycle) % slot_num] = -1;
        }
    }
    for (int k = 0; k < node_num; k++)
    {
        ch.path[index][k] = -1;
    }
}
void clearOne(chromosome& ch, int index)
{

    //函数：删除一个个体中流的配置
    //输入：
    //chromosome& ch:个体的引用
    //const int index:流的序号
    ch.isset[index] = false;
    vector<int> route;
    int i = 0;
    while (ch.path[index][i] != stream[index].dst)
    {
        route.push_back(ch.path[index][i++]);
    }
    route.push_back(stream[index].dst);
    for (int j = 0; j < route.size() - 1; j++)
    {
        for (int k = 0; k < slot_num / stream[index].cycle; k++)
        {
            ch.portslot[route[j]][route[j + 1]][(ch.offset[index] + j + k * stream[index].cycle) % slot_num] = -1;
        }
    }
    for (int k = 0; k < node_num; k++)
    {
        ch.path[index][k] = -1;
    }
}
void crossover(chromosome* population_next)
{
    //函数：交叉操作
    //cout<<"enter crossover"<<endl;
    double rate_rand = 0;
    srand((unsigned)time(NULL));
    //如果种群数是偶数，保留一个最优个体
    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    sort(population_next, population_next + population_size, cmp);
    if (population_size % 2)
    {
        shuffle(population_next, population_next + population_size - 1, default_random_engine(seed));
    }
    else
    {
        shuffle(population_next, population_next + population_size - 2, default_random_engine(seed));
    }
    for (int j = 0; j < (population_size - 1) / 2; j++)
    {
        rate_rand = (float)rand() / (RAND_MAX);
        //如果大于交叉概率，则交换两个种群中随机一条流的全部信息
        if (rate_rand <= rate_crossover)
        {
            int pop1 = j; //随机挑选出的两个种群
            int pop2 = (population_size - 1) / 2 - 1 - j;
            int stream_rand = (int)rand() % (stream_num); //随机交换的流
            //cout<<"pick population "<<pop1<<" and "<<pop2<<",";
            //如果随机流在种群1中放了而在种群2中没放
            if (population_next[pop1].isset[stream_rand] && !population_next[pop2].isset[stream_rand])
            {
                vector<int> route;
                int i = 0;
                while (population_next[pop1].path[stream_rand][i] != stream[stream_rand].dst && i<node_num)
                {
                    route.push_back(population_next[pop1].path[stream_rand][i++]);
                }
                route.push_back(stream[stream_rand].dst);
                clearOne(population_next[pop1], stream_rand, route);
                setOne(population_next[pop2], stream_rand, route, population_next[pop1].offset[stream_rand]);
            }
            //如果随机流在种群2中放了而在种群1中没放
            else if (!population_next[pop1].isset[stream_rand] && population_next[pop2].isset[stream_rand])
            {
                vector<int> route;
                int i = 0;
                while (population_next[pop2].path[stream_rand][i] != stream[stream_rand].dst)
                {
                    route.push_back(population_next[pop2].path[stream_rand][i++]);
                }
                route.push_back(stream[stream_rand].dst);
                clearOne(population_next[pop2], stream_rand, route);
                setOne(population_next[pop1], stream_rand, route, population_next[pop2].offset[stream_rand]);
            }
            //如果在两个种群中都放了
            else if (population_next[pop1].isset[stream_rand] && population_next[pop2].isset[stream_rand])
            {
                vector<int> route1;
                vector<int> route2;
                int i = 0;
                while (population_next[pop1].path[stream_rand][i] != stream[stream_rand].dst && i<=node_num)
                {
                    route1.push_back(population_next[pop1].path[stream_rand][i++]);
                }
                route1.push_back(stream[stream_rand].dst);
                i = 0;
                while (population_next[pop2].path[stream_rand][i] != stream[stream_rand].dst)
                {
                    route2.push_back(population_next[pop2].path[stream_rand][i++]);
                }
                route2.push_back(stream[stream_rand].dst);
                clearOne(population_next[pop1], stream_rand, route1);
                clearOne(population_next[pop2], stream_rand, route2);
                setOne(population_next[pop2], stream_rand, route1, population_next[pop1].offset[stream_rand]);
                setOne(population_next[pop1], stream_rand, route2, population_next[pop2].offset[stream_rand]);
            }
            else
            {

            }
        }
    }
}
void mutation(chromosome* population_next)
{

    // 函数：变异操作
    //cout<<"enter mutation"<<endl;
    double rate_rand = 0;
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        rate_rand = (float)rand() / (RAND_MAX);
        if (rate_rand <= rate_mutation)
        {
            //cout<<" decide to mutate"<<endl;
            int stream_rand = (int)rand() % stream_num;
            if (population_next[i].isset[stream_rand])
            {
                clearOne(population_next[i], stream_rand);
            }
            else
            {
                //cout<<"wasn't set"<<endl;
                population_next[i].offset[stream_rand] = rand() % stream[stream_rand].cycle;
                bool found = false;
                vector<int> path; //路径的临时变量
                path.push_back(stream[stream_rand].src);
                vector<vector<bool> > visited(node_num, vector<bool>(node_num, false));
                vector<int> ans; //计算出的随机路径
                randomRouting(found, stream[stream_rand].dst, path, visited, ans);
                //判断路径是否满足时延要求
                if ((rand() % 20 + (ans.size() - 1) * 10) <= stream[stream_rand].bound)
                {
                    setOne(population_next[i], stream_rand, ans, population_next[i].offset[stream_rand]);
                }
            }
        }
    }
}
void op_crossover(chromosome* population_next)
{

    //函数：交叉操作
    //cout<<"enter op_crossover"<<endl;
    double rate_rand = 0;
    srand((unsigned)time(NULL));
    //如果种群数是偶数，保留一个最优个体
    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    sort(population_next, population_next + population_size, cmp);
    if (population_size % 2)
    {
        shuffle(population_next, population_next + population_size - 1, default_random_engine(seed));
    }
    else
    {
        shuffle(population_next, population_next + population_size - 2, default_random_engine(seed));
    }
    for (int j = 0; j < (population_size - 1) / 2; j++)
    {
        rate_rand = (float)rand() / (RAND_MAX);
        //如果大于交叉概率，则交换两个种群中随机一条流的全部信息
        if (rate_rand <= rate_crossover)
        {
            int pop1 = j; //随机挑选出的两个种群
            int pop2 = (population_size - 1) / 2 - 1 - j;
            int stream_rand = (int)rand() % (stream_num); //随机交换的流
            //如果随机流在种群1中放了而在种群2中没放
            if (population_next[pop1].isset[stream_rand] && !population_next[pop2].isset[stream_rand])
            {
                vector<int> route;
                int i = 0;
                while (population_next[pop1].path[stream_rand][i] != stream[stream_rand].dst)
                {
                    route.push_back(population_next[pop1].path[stream_rand][i++]);
                }
                route.push_back(stream[stream_rand].dst);
                //clearOne(population_next[pop1],stream_rand,route);
                setOne(population_next[pop2], stream_rand, route, population_next[pop1].offset[stream_rand]);
            }
            //如果随机流在种群2中放了而在种群1中没放
            else if (!population_next[pop1].isset[stream_rand] && population_next[pop2].isset[stream_rand])
            {
                vector<int> route;
                int i = 0;
                while (population_next[pop2].path[stream_rand][i] != stream[stream_rand].dst)
                {
                    route.push_back(population_next[pop2].path[stream_rand][i++]);
                }
                route.push_back(stream[stream_rand].dst);
                //clearOne(population_next[pop2],stream_rand,route);
                setOne(population_next[pop1], stream_rand, route, population_next[pop2].offset[stream_rand]);
            }
            //如果在两个种群中都放了
            else if (population_next[pop1].isset[stream_rand] && population_next[pop2].isset[stream_rand])
            {

            }
            else
            {
                //cout<<"both not settled.Do nothing."<<endl;
            }
        }
    }
}
void op_mutation(chromosome* population_next)
{

    // 函数：变异操作
    //cout<<"enter op_mutation"<<endl;
    double rate_rand = 0;
    srand((unsigned)time(NULL));
    for (int i = 0; i < population_size; i++)
    {
        rate_rand = (float)rand() / (RAND_MAX);
        if (rate_rand <= rate_mutation)
        {
            int stream_rand = (int)rand() % stream_num;
            if (population_next[i].isset[stream_rand])
            {
                //clearOne(population_next[i],stream_rand);
            }
            else
            {
                population_next[i].offset[stream_rand] = rand() % stream[stream_rand].cycle;
                bool found = false;
                vector<int> path; //路径的临时变量
                path.push_back(stream[stream_rand].src);
                vector<vector<bool> > visited(node_num, vector<bool>(node_num, false));
                vector<int> ans; //计算出的随机路径
                randomRouting(found, stream[stream_rand].dst, path, visited, ans);
                //判断路径是否满足时延要求
                if ((rand() % 20 + (ans.size() - 1) * 10) <= stream[stream_rand].bound)
                {
                    setOne(population_next[i], stream_rand, ans, population_next[i].offset[stream_rand]);
                }
            }
        }
    }
}
/*void topoInit1(){
    m[1][2] = true;    m[2][1] = true;
    m[2][3] = true;    m[3][2] = true;
    m[3][4] = true;    m[4][3] = true;
    m[5][6] = true;    m[6][5] = true;
    m[6][7] = true;    m[7][6] = true;
    m[7][8] = true;    m[8][7] = true;
    m[1][5] = true;    m[5][1] = true;
    m[2][6] = true;    m[6][2] = true;
    m[3][7] = true;    m[7][3] = true;
    m[4][8] = true;    m[8][4] = true;
    stream[0] = {1,8,3,20};
    stream[1] = {1,4,3,7};
    stream[2] = {8,5,4,4};
    stream[3] = {2,7,6,5};
    stream[4] = {5,3,2,10};
    stream[5] = {4,5,2,7};
    stream[6] = {4,5,4,10};
    stream[7] = {1,7,3,8};
    stream[8] = {3,8,4,5};
    stream[9] = {3,5,6,6};
}

void topoInit2(){
    m[0][1] = true; m[1][0] = true;
    m[1][2] = true; m[2][1] = true;
    m[2][3] = true; m[3][2] = true;
    m[4][5] = true; m[5][4] = true;
    m[5][6] = true; m[6][5] = true;
    m[6][7] = true; m[7][6] = true;
    m[1][5] = true; m[5][1] = true;
    m[2][6] = true; m[6][2] = true;

    stream[0] = {0,3,3,10};
    stream[1] = {0,7,4,15};
    stream[2] = {7,4,4,8};
    stream[3] = {7,3,6,6};
}*/
/*const int stream_num = 20;  //流数
const int node_num = 41;    //网络节点数
bool m[node_num][node_num];   //拓扑
Stream stream[stream_num];   //流集合*/
bool ifinit = false;
void topoInit()
{
    if (!ifinit)
    {
        if (node_num == 24) {//US net
            m[0][1] = 1; m[1][0] = 1;
            m[0][5] = 1; m[5][0] = 1;
            m[1][5] = 1; m[5][1] = 1;
            m[1][2] = 1; m[2][1] = 1;
            m[2][3] = 1; m[3][2] = 1;
            m[2][4] = 1; m[4][2] = 1;
            m[2][6] = 1; m[6][2] = 1;
            m[3][4] = 1; m[4][3] = 1;
            m[3][6] = 1; m[6][3] = 1;
            m[4][7] = 1; m[7][4] = 1;
            m[5][6] = 1; m[6][5] = 1;
            m[5][8] = 1; m[8][5] = 1;
            m[5][10] = 1; m[10][5] = 1;
            m[6][8] = 1; m[8][6] = 1;
            m[6][7] = 1; m[7][6] = 1;
            m[7][9] = 1; m[9][7] = 1;
            m[8][9] = 1; m[9][8] = 1;
            m[8][10] = 1; m[10][8] = 1;
            m[8][11] = 1; m[11][8] = 1;
            m[9][12] = 1; m[12][9] = 1;
            m[9][13] = 1; m[13][9] = 1;
            m[10][18] = 1; m[18][10] = 1;
            m[10][11] = 1; m[11][10] = 1;
            m[10][14] = 1; m[14][10] = 1;
            m[11][15] = 1; m[15][11] = 1;
            m[11][12] = 1; m[12][11] = 1;
            m[12][13] = 1; m[13][12] = 1;
            m[12][16] = 1; m[16][12] = 1;
            m[13][17] = 1; m[17][13] = 1;
            m[14][19] = 1; m[19][14] = 1;
            m[14][15] = 1; m[15][14] = 1;
            m[15][20] = 1; m[20][15] = 1;
            m[15][21] = 1; m[21][15] = 1;
            m[15][16] = 1; m[16][15] = 1;
            m[16][17] = 1; m[17][16] = 1;
            m[16][21] = 1; m[21][16] = 1;
            m[16][22] = 1; m[22][16] = 1;
            m[17][23] = 1; m[23][17] = 1;
            m[18][19] = 1; m[19][18] = 1;
            m[19][20] = 1; m[20][19] = 1;
            m[20][21] = 1; m[21][20] = 1;
            m[21][22] = 1; m[22][21] = 1;
            m[22][23] = 1; m[23][22] = 1;
        }
        if (node_num == 14) {
            m[0][1] = 1; m[1][0] = 1;
            m[0][2] = 1; m[2][0] = 1;
            m[0][3] = 1; m[3][0] = 1;
            m[1][2] = 1; m[2][1] = 1;
            m[1][7] = 1; m[7][1] = 1;
            m[2][4] = 1; m[4][2] = 1;
            m[3][5] = 1; m[5][3] = 1;
            m[3][10] = 1; m[10][3] = 1;
            m[4][5] = 1; m[5][4] = 1;
            m[4][8] = 1; m[8][4] = 1;
            m[4][12] = 1; m[12][4] = 1;
            m[5][6] = 1; m[6][5] = 1;
            m[6][7] = 1; m[7][6] = 1;
            m[7][9] = 1; m[9][7] = 1;
            m[8][9] = 1; m[9][8] = 1;
            m[9][11] = 1; m[11][9] = 1;
            m[9][13] = 1; m[13][9] = 1;
            m[10][11] = 1; m[11][10] = 1;
            m[10][13] = 1; m[13][10] = 1;
            m[11][12] = 1; m[12][11] = 1;
            m[12][13] = 1; m[13][12] = 1;

        }
        else {
            //cout << "node_num:" << node_num << "        stream_num:" << stream_num << endl;
            srand((unsigned)time(NULL));
            for (int i = 0; i < node_num; i++)
            {
                for (int j = i + 1; j < node_num; j++)
                {
                    double r = 0;
                    r = (float)rand() / (RAND_MAX);
                    if (r > 0.5)
                    {
                        m[i][j] = 1;
                        m[j][i] = 1;
                    }
                }
            }
        }
        for (int i = 0; i < node_num; i++)
        {
            for (int j = i + 1; j < node_num; j++)
            {
                if (m[i][j]==1)
                {
                    m[i][j] += rand() % 4;
                    m[j][i] = m[i][j];
                }
            }
        }
        ifinit = true;
    }
}
void streaminit()
{
    int cycle[6] = { 2, 2, 3, 4, 6, 6 };
    for (int i = 0; i < stream_num; i++)
    {
        stream[i].src = (rand() % node_num);
        stream[i].dst = (rand() % node_num);
        while (stream[i].dst == stream[i].src)
        {
            stream[i].dst = rand() % node_num;
        }
        stream[i].cycle = cycle[rand() % 6];
        stream[i].bound = 50;
    }
}
chromosome population_next_generation[population_size];
chromosome population_current[population_size];
chromosome best_individual;
chromosome best_individual2;
chromosome zeros_chromosome;
int main(){
    FILE* fp;
    fopen_s(&fp, "C:\\Users\\wwh\\Desktop\\paper\\simulation\\a.txt", "w+"); 
    char out[3]="";
    cout << "MAIN" << endl;
    topoInit();
    streaminit();
    int b1 = 0;
    int b2 = 0;
    for (int b = 3; b < 4; b++)
    {
        for (int c = 0; c < stream_num; c++)
        {
            stream[c].bound = rand() % 70 + 10;// b * 10 + 5;
        }
        //cout<<"bound is "<<b*10<<endl;
        for (int i = 0; i < 10; i++)
        {
          

            for (int i = 0; i < stream_num; i++)
            {
                zeros_chromosome.isset[i] = 0;
                zeros_chromosome.offset[i] = 0;
                for (int j = 0; j < node_num; j++)
                {
                    zeros_chromosome.path[i][j] = 0;
                }
            }
            zeros_chromosome.fitness = 0;
            zeros_chromosome.rate_fit = 0;
            zeros_chromosome.cumu_fit = 0;
            best_individual = zeros_chromosome;
            for (int i = 0; i < population_size; i++)
            {
                population_current[i] = zeros_chromosome;
                population_next_generation[i] = zeros_chromosome;
            }
            //cout<<"请输入迭代数:";
            //int iteration_num;
            //cin>>iteration_num;
            //switchInit();
            auto t1 = chrono::steady_clock::now();
            populationInit(population_current);//
            /*for(int i=0;i<population_size;i++){
            showSlot(&population_current[i]);
    }*/
            freshPro(population_current);
            //cout << "start" << best_individual.fitness << endl;
            for (int i = 0; i < population_size; i++) {
                if (population_current[i].fitness > best_individual.fitness) {
                    best_individual = population_current[i];
                }
            }
            for (int i = 0; i < iteration_num; i++)
            {
                //cout<<"----------iteration num "<<i<<" ------------------"<<endl;
                selectPrw(population_current, population_next_generation, best_individual,(double)i);
                op_crossover(population_next_generation);
                op_mutation(population_next_generation);
                freshPro(population_next_generation);
                int mean = 0;
                for (int j = 0; j < population_size; j++)
                {
                    mean += population_current[j].fitness;
                    population_current[j] = population_next_generation[j];
                    population_next_generation[j] = zeros_chromosome;
                    //cout << population_current[i].fitness << ' ';
                    //fprintf(fp, "%d\n", population_current[i].fitness);
                }
                //cout<<best_individual.fitness<<endl;
                b1 = max(best_individual.fitness, b1);
                //printf("%.6f\n", float(mean) / population_size);
                //cout << b1 << endl;
                //cout << endl;
            }
            //cout<<endl;
            //cout<<"最优个体的放置流数为"<<best_individual.fitness<<endl;
            int ocuppy = 0;
            int total = 0;
            for (int i = 0; i < node_num;i++)
                for (int j = 0; j < node_num; j++) {
                    if (m[i][j] != 0) total += 1;
                    for (int k = 0; k < slot_num; k++) {
                        if (best_individual.portslot[i][j][k] > 0)
                            ocuppy += 1;
                    }
                }
                    


            cout << endl;
            int s = 0;
            int hopstotle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                if (best_individual.isset[i])
                {
                    ++s;
                    //cout << "stream " << i << " is settled.offset = " << best_individual.offset[i] << ". cycle = " << stream[i].cycle << ". " << stream[i].src << "->" << stream[i].dst << " .And route is ";
                    int hops = 0;
                    for (int j = 0; j < node_num; j++)
                    {
                        //cout << best_individual.path[i][j] << " ";
                        if (best_individual.path[i][j] == stream[i].dst)
                            break;
                        hops++;
                    }
                    hopstotle += hops;
                    cout << hops << endl;
                }
            }
            cout << endl;
            //cout << double(hopstotle)/s <<'\n';
            auto t2 = chrono::steady_clock::now();
            //cout << double(ocuppy) / (slot_num * total) << endl;
            //cout << s << endl;
            chrono::duration<double> time_span = chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            //std::cout << time_span.count() << endl;
            //showSlot(&best_individual);



            int thoughput_now = 0;
            int thoughput_totle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                thoughput_totle += 1000 / stream[i].cycle; //mbps
                if (best_individual.isset[i])
                {
                    thoughput_now += 1000 / stream[i].cycle;
                }
            }
            //cout << "gdnts" << double(thoughput_now) << endl;
            //chrono::duration<double> time_span = chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            //std::cout << time_span.count() << endl;
            //showSlot(&best_individual);


            b1 = 0;


            //最短路路由算法
            for (int i = 0; i < stream_num; i++)
            {
                zeros_chromosome.isset[i] = 0;
                zeros_chromosome.offset[i] = 0;
                for (int j = 0; j < node_num; j++)
                {
                    zeros_chromosome.path[i][j] = 0;
                }
            }
            zeros_chromosome.fitness = 0;
            zeros_chromosome.rate_fit = 0;
            zeros_chromosome.cumu_fit = 0;
            for (int i = 0; i < population_size - 4000; i++)
            {
                population_current[i] = zeros_chromosome;
                population_next_generation[i] = zeros_chromosome;
            }
            auto t3 = chrono::steady_clock::now();
            sht_populationInit(population_current);/*
                                                  gg
                                                  */
            freshPro(population_current);
            for (int i = 0; i < iteration_num; i++)
            {
                //cout<<"----------iteration num "<<i<<" ------------------"<<endl;
                selectPrw2(population_current, population_next_generation, best_individual2);
                op_crossover(population_next_generation);
                //op_mutation(population_next_generation);
                freshPro(population_next_generation);
                for (int i = 0; i < population_size; i++)
                {
                    population_current[i] = population_next_generation[i];
                    population_next_generation[i] = zeros_chromosome;
                }
                //cout<<"best = "<<best_individual.fitness<<endl;
            }

            ocuppy = 0;
            total = 0;
            for (int i = 0; i < node_num; i++)
                for (int j = 0; j < node_num; j++) {
                    if (m[i][j] != 0) total += 1;
                    for (int k = 0; k < slot_num; k++) {
                        if (best_individual2.portslot[i][j][k] > 0)
                            ocuppy += 1;
                    }
                }
            cout << endl;
            s = 0;
            hopstotle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                if (best_individual2.isset[i])
                {
                    ++s;
                    //cout << "stream " << i << " is settled.offset = " << best_individual2.offset[i] << ". cycle = " << stream[i].cycle << ". " << stream[i].src << "->" << stream[i].dst << " .And route is ";
                    int hops = 0;
                    for (int j = 0; j < node_num; j++)
                    {
                        //cout << best_individual2.path[i][j] << " ";
                        if (best_individual2.path[i][j] == stream[i].dst)
                            break;
                        hops++;
                    }
                    hopstotle += hops;
                    cout << hops << endl;;
                    //cout << endl;
                }
            }
            cout << endl;
            //cout << double(hopstotle) / s << '\n';
            //cout << double(ocuppy) / (slot_num * total) << endl;
            //cout << s << endl;


            thoughput_now = 0;
            thoughput_totle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                thoughput_totle += 1000 / stream[i].cycle; //mbps
                if (best_individual2.isset[i])
                {
                    thoughput_now += 1000 / stream[i].cycle;
                }
            }
            cout << "sp:" << double(thoughput_now) << endl;
            //chrono::duration<double> time_span = chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            //std::cout << time_span.count() << endl;
            //showSlot(&best_individual);
            
            //cout << "--------------------" << endl;
            //auto t4 = chrono::steady_clock::now();
            //chrono::duration<double> time_span2 = chrono::duration_cast<std::chrono::duration<double>>(t4 - t3);
            //std::cout << "It took me " << time_span.count() << " seconds." << endl;
            //std::cout << time_span2.count() << endl;
            //showSlot(&best_individual);

            //负载平衡路由算法
            for (int i = 0; i < stream_num; i++)
            {
                zeros_chromosome.isset[i] = 0;
                zeros_chromosome.offset[i] = 0;
                for (int j = 0; j < node_num; j++)
                {
                    zeros_chromosome.path[i][j] = 0;
                }
            }
            zeros_chromosome.fitness = 0;
            zeros_chromosome.rate_fit = 0;
            zeros_chromosome.cumu_fit = 0;
            best_individual2 = zeros_chromosome;
            for (int i = 0; i < population_size; i++)
            {
                population_current[i] = zeros_chromosome;
                population_next_generation[i] = zeros_chromosome;
            }
            auto t5 = chrono::steady_clock::now();
            lb_populationInit(population_current);
            freshPro(population_current);
            for (int i = 0; i < iteration_num-700; i++)
            {
                //cout<<"----------iteration num "<<i<<" ------------------"<<endl;
                selectPrw2(population_current, population_next_generation, best_individual2);
                op_crossover(population_next_generation);
                //op_mutation(population_next_generation);
                freshPro(population_next_generation);
                for (int i = 0; i < population_size; i++)
                {
                    population_current[i] = population_next_generation[i];
                    population_next_generation[i] = zeros_chromosome;
                }
                //cout<<"best = "<<best_individual.fitness<<endl;
            }


            thoughput_now = 0;
            thoughput_totle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                thoughput_totle += 1000 / stream[i].cycle; //mbps
                if (best_individual2.isset[i])
                {
                    thoughput_now += 1000 / stream[i].cycle;
                }
            }
            cout << "lb:" << double(thoughput_now) << endl;

            ocuppy = 0;
            total = 0;
            for (int i = 0; i < node_num; i++)
                for (int j = 0; j < node_num; j++) {
                    if (m[i][j] != 0) total += 1;
                    for (int k = 0; k < slot_num; k++) {
                        if (best_individual2.portslot[i][j][k] > 0)
                            ocuppy += 1;
                    }
                }
            cout << endl;
            s = 0;
            hopstotle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                if (best_individual2.isset[i])
                {
                    ++s;
                    //cout << "stream " << i << " is settled.offset = " << best_individual2.offset[i] << ". cycle = " << stream[i].cycle << ". " << stream[i].src << "->" << stream[i].dst << " .And route is ";
                    int hops = 0;
                    for (int j = 0; j < node_num; j++)
                    {
                        //cout << best_individual2.path[i][j] << " ";
                        if (best_individual2.path[i][j] == stream[i].dst)
                            break;
                        hops++;
                    }
                    hopstotle += hops;
                    cout << hops << endl;
                    //cout << endl;
                }
            }
            cout << endl;
            //cout << double(hopstotle) / s << '\n';
            //cout << double(ocuppy) / (slot_num * total) << endl;
            //cout << s << endl;
            //cout << "--------------------" << endl;
           // auto t6 = chrono::steady_clock::now();
            //chrono::duration<double> time_span3 = chrono::duration_cast<std::chrono::duration<double>>(t6 - t5);
            //std::cout << "It took me " << time_span.count() << " seconds." << endl;
            //std::cout << time_span3.count() << endl;
            //showSlot(&best_individual);
            // 
            // 
            //固定路由算法
            return 0;
            for (int i = 0; i < stream_num; i++)
            {
                zeros_chromosome.isset[i] = 0;
                zeros_chromosome.offset[i] = 0;
                for (int j = 0; j < node_num; j++)
                {
                    zeros_chromosome.path[i][j] = 0;
                }
            }
            zeros_chromosome.fitness = 0;
            zeros_chromosome.rate_fit = 0;
            zeros_chromosome.cumu_fit = 0;
            for (int i = 0; i < population_size; i++)
            {
                population_current[i] = zeros_chromosome;
                population_next_generation[i] = zeros_chromosome;
            }
            auto t7 = chrono::steady_clock::now();
            frpopulationInit(population_current);
            freshPro(population_current);
            for (int i = 0; i < iteration_num - 4000; i++)
            {
                //cout<<"----------iteration num "<<i<<" ------------------"<<endl;
                selectPrw2(population_current, population_next_generation, best_individual2);
                op_crossover(population_next_generation);
                //op_mutation(population_next_generation);
                freshPro(population_next_generation);
                for (int i = 0; i < population_size; i++)
                {
                    population_current[i] = population_next_generation[i];
                    population_next_generation[i] = zeros_chromosome;
                }
                //cout<<"best = "<<best_individual.fitness<<endl;
            }
            s = 0;
            hopstotle = 0;
            for (int i = 0; i < stream_num; i++)
            {
                if (best_individual2.isset[i])
                {
                    ++s;
                    //cout << "stream " << i << " is settled.offset = " << best_individual2.offset[i] << ". cycle = " << stream[i].cycle << ". " << stream[i].src << "->" << stream[i].dst << " .And route is ";
                    int hops = 0;
                    for (int j = 0; j < node_num; j++)
                    {
                        //cout << best_individual2.path[i][j] << " ";
                        if (best_individual2.path[i][j] == stream[i].dst)
                            break;
                        hops++;
                    }
                    hopstotle += hops;
                    //cout << hops;
                    //cout << endl;
                }
            }
            //cout << double(hopstotle) / s << '\n';
            //cout << s << endl;
            //cout << "--------------------" << endl;
            auto t8 = chrono::steady_clock::now();
            chrono::duration<double> time_span4 = chrono::duration_cast<std::chrono::duration<double>>(t8 - t7);
            //std::cout << "It took me " << time_span.count() << " seconds." << endl;
            //std::cout << time_span4.count() << endl;
            //showSlot(&best_individual);
        }

        //cout << "node_num:" << node_num << "        stream_num:" << stream_num << " bound is " << b * 10 << endl;
        //cout << "AAA " << b1 << " ,FR " << b2 << endl;
    }

    return 0;
}
