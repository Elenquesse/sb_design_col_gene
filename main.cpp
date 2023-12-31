﻿#include "MainModel.h"
#include "LppModel.h"

using namespace std;

int main(int argc, char *argv[])
{
    ArcSets arcSets;
    LineSets lineSets;
    ODSets odSets;

    /*-------------------------------------------模型需要的各种参数------------------------------------------------------*/
{
    // 把 arc 加进去
    /*
    arcSets.addArc(0, 2, 1, 10, 1.0, METRO_ARC);
    arcSets.addArc(2, 3, 1, 10, 1.0, METRO_ARC);
    arcSets.addArc(1, 2, 1, 10, 1.0, METRO_ARC);
    arcSets.addArc(1, 4, 8, 50, 1.0, BUS_ARC);
    arcSets.addArc(3, 4, 9, 50, 1.0, BUS_ARC);
    */
    arcSets.readArcsFromFile("arcSetsSetting.in");

/*
    // 加入空的 line
    for (int i = 0; i < 1; ++i) lineSets.addLine(BUS_LINE);
    for (int i = 0; i < 2; ++i) lineSets.addLine(METRO_LINE);

    // 配置各 line 经过的 arc
    lineSets.setLinePass(0, 4);
//    lineSets.setLinePass(1, 3);
    lineSets.setLinePass(1, arcSets.getArcId(0, 2));
    lineSets.setLinePass(1, 1);
    lineSets.setLinePass(2, 2);
    lineSets.setLinePass(2, 1);
*/
    lineSets.readLinesFromFile("lineSetsSetting.in", arcSets);

/*
    // 加入 OD
    odSets.addOdPair(0, 4, 127);
    odSets.addOdPair(1, 4, 319);

    // 给各 OD 加入 path
    odSets.addPathToOd(0);
    odSets.addPathToOd(1);
    odSets.addPathToOd(1);

    // 配置各 OD 的 path 经过的 arc
    odSets.letPathPassArc(0, 0, arcSets.getArcId(0, 2));
    odSets.letPathPassArc(0, 0, arcSets.getArcId(2, 3));
    odSets.letPathPassArc(0, 0, arcSets.getArcId(3, 4));
    odSets.letPathPassArc(1, 0, arcSets.getArcId(1, 2));
    odSets.letPathPassArc(1, 0, arcSets.getArcId(2, 3));
    odSets.letPathPassArc(1, 0, arcSets.getArcId(3, 4));
    odSets.letPathPassArc(1, 1, arcSets.getArcId(1, 4));
*/
    odSets.readOdsFromFile("odSetsSetting.out", arcSets);


    // 计算c_l
    for (int i: lineSets.getLines()) {
        for (int j: arcSets.getBusArcs()) {
            lineSets.cl[i] += alpha * (double) lineSets.miu(i, j) * (double) arcSets.getArcDis(j);
        }
    }

    // 计算c_{k,p}
    for (int k: odSets.getOds()) {
        vector<double> ckp;
        for (int p = 0; p < odSets.getPathNums(k); ++p) {
            double tmp = 0.0;
            for (int e: arcSets.getArcs()) {
                tmp += (double)arcSets.getArcDis(e) * odSets.niu(k, p, e) / arcSets.getVelocity(e);
            }
            ckp.push_back(tmp);
        }
        odSets.ckp.push_back(ckp);
    }
}

//    freopen("output.txt", "w", stdout);
//    ios::sync_with_stdio(false);

    /*--------------------------------------------下面开始搭模型--------------------------------------------------------*/
    try{

        bool flag = true;

        while (true){
            // 造RMP
            MainModel m = MainModel();
            m.addVars(lineSets, odSets);
            m.toCont();  // 需要对偶的时候用松弛模型
            m.setObjective(lineSets, odSets);
            m.addConstrains(arcSets, lineSets, odSets);
//            m.model.set(GRB_IntParam_OutputFlag, 0);
            m.optimize();
//            m.model.computeIIS();
//            m.model.write("main.ilp");
            cout << "------------------------ fuckme: x_l start ---------------" << endl << endl;
            vector< vector<double> > vet = m.getDualX(arcSets);
            vector<odp> new_line;
            int num_of_nodes = (int)vet.size();
            double ans = -INF;
            flag = false;
            // 在xl上找新最长路
            for (int i = 0; i < num_of_nodes; ++i){
                for (int j = 0; j < num_of_nodes; ++j){
                    if (i == j) continue;
                    LppModel lm = LppModel(vet, 5);
                    lm.setOrigin(i);  lm.setDestination(j);
                    lm.setBias(0);
                    lm.buildModel();
                    double tmp = lm.solve();
                    if(tmp > ans && tmp > 0){
                        ans = tmp;
                        new_line = lm.getResult();
                        flag = true;
                        lm.printResult();
                        cout << tmp;
                    }
                }
            }

            if (!flag) break; // 没找到大于0的最长路直接完蛋

            // 把最长路更新进lineset
            int new_line_id = lineSets.addLine(BUS_LINE);
            for (auto iter : new_line){
                lineSets.setLinePass(new_line_id, arcSets.getArcId(iter.first, iter.second));
            }

            // 补算c_l
            for (int j: arcSets.getBusArcs()) {
                lineSets.cl[new_line_id] += alpha * (double) lineSets.miu(new_line_id, j) * (double) arcSets.getArcDis(j);
            }
        }

        cout << "--------------------fuckme: z_k,l strat --------------------\n";
        int fffffffffffffff = 0;
        // z_k,l部分的列生成
        while (true){
            // 造RMP
            MainModel m = *new MainModel();
            m.addVars(lineSets, odSets);
            m.toCont();  // 需要对偶的时候用松弛模型
            m.setObjective(lineSets, odSets);
            m.addConstrains(arcSets, lineSets, odSets);
//            m.model.set(GRB_IntParam_OutputFlag, 0);
            m.optimize();

            vector<odp> new_line;
            double ans = -INF;
            flag = false;

            // 在z_k上找最短路，遍历每个k给出一个最长的
            for (int k : odSets.getOds()){
                cout << "k = " << k << " in z_k,l" << endl;
                vector< vector<double> > vet = m.getDualZ(arcSets, odSets, k);
                int num_of_nodes = (int)vet.size();
                for (int fuckme : odSets.getOds()) { //试一下只找起终点是OD的里面的最长路往结果里加
                    int i = odSets.origin[fuckme], j = odSets.destination[fuckme];
//                for (int i = 0; i < num_of_nodes; ++i){
//                    for (int j = 0; j < num_of_nodes; ++j){
//                        if (i == j) continue;
                    LppModel lm = LppModel(vet, 10);
                    lm.setOrigin(i);
                    lm.setDestination(j);
                    lm.setBias(phi3 * odSets.getDemand(k)); // 最后要-phi_3 * q_k
                    lm.buildModel();
                    double tmp = lm.solve();
                    if (tmp > ans && tmp > 0) {
                        ans = tmp;
                        new_line = lm.getResult();
                        flag = true;
                        lm.printResult();
                    }
                }
//                    }
//                }
            }

            if (!flag) break; // 没找到大于0的最长路直接完蛋

            // 把最长路更新进lineset
            int new_line_id = lineSets.addLine(BUS_LINE);
            for (auto iter : new_line){
                lineSets.setLinePass(new_line_id, arcSets.getArcId(iter.first, iter.second));
            }
            // 补算c_l
            for (int j: arcSets.getBusArcs()) {
                lineSets.cl[new_line_id] += alpha * (double) lineSets.miu(new_line_id, j) * (double) arcSets.getArcDis(j);
            }
        }

        MainModel m = MainModel();
        m.addVars(lineSets, odSets);
        m.setObjective(lineSets, odSets);
        m.addConstrains(arcSets, lineSets, odSets);
        m.optimize();

        m.printResult(lineSets, odSets);

        for (int i : lineSets.getLines()){
            for (int e : arcSets.getArcs()){
                cout << lineSets.miu(i, e) << " ";
            }
            cout << endl;
        }

    } catch (GRBException &e) {
        cout << "Error code = " << e.getErrorCode() << endl;
        cout << e.getMessage() << endl;
    } catch (...) {
        cout << "Exception during optimization" << endl;
    }

    return 0;
}