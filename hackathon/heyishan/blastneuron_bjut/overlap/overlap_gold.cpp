/* overlap_gold.cpp
 * This is a plugin to find motif from local_alignments or reconstructions.
 * 2017-03-01 : by Yishan He
 */
#include<iostream>
#include<basic_surf_objs.h>
#include"overlap_gold.h"
#include"../../../../released_plugins/v3d_plugins/neurontracing_vn2/neuron_radius/marker_radius.h"
#include <stdio.h>
#include"sort_swc.h"
using namespace std;

template<class T> T pow2(T a)
{
    return a*a;
}

bool export_neuronList2file(QList<NeuronSWC> & lN, QString fileSaveName)
{
    QFile file(fileSaveName);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Text))
        return false;
    QTextStream myfile(&file);
    myfile<<"# generated by Vaa3D Plugin overlap_gold"<<endl;
    myfile<<"# id,type,x,y,z,r,pid"<<endl;
    for (V3DLONG i=0;i<lN.size();i++)
        myfile << lN.at(i).n <<" " << lN.at(i).type << " "<< lN.at(i).x <<" "<<lN.at(i).y << " "<< lN.at(i).z << " "<< lN.at(i).r << " " <<lN.at(i).pn << "\n";

    file.close();
    cout<<"swc file "<<fileSaveName.toStdString()<<" has been generated, size: "<<lN.size()<<endl;
    return true;
}

bool evaluate_radius(vector<MyMarker*> & inswc, QString raw_img, V3DPluginCallback2 &callback)
{
    double bkg_thresh = 40;
    string inimg_file = raw_img.toStdString();
    unsigned char * inimg1d = 0;
    V3DLONG in_sz[4];
    int datatype;
    if(!simple_loadimage_wrapper(callback,(char *)inimg_file.c_str(), inimg1d, in_sz, datatype))  {cout<<"load img wrong"<<endl;  return false;}
    if(inswc.empty())  {cout<<"inswc is empty"<<endl; return false;}
    for(int i = 0; i < inswc.size(); i++)
    {
        MyMarker * marker = inswc[i];
        marker->radius = markerRadius(inimg1d, in_sz, *marker, bkg_thresh);
    }
    return true;
}

bool prune_short_tree(vector<MyMarker*> & inswc, vector<MyMarker*> & outswc, double prune_thres)
{
    for(int i=0; i<inswc.size(); i++)
    {
        int count = 0;
        for(int j=i; j<inswc.size(); j++)
        {
            if(inswc[j]->parent==0 && count==0)    count = 1;
            else if(inswc[j]->parent==0 && count != 0)   break;
            else count += 1;
        }
        if(count>prune_thres)
        {
            for(int j=0 ; j<count; j++)
            {
                outswc.push_back(inswc[i+j]);
            }
        }
        i += count - 1;
    }
}

// reference: inter_node_pruning_plugin.cpp
bool inter_node_pruning(NeuronTree & nt, vector<MyMarker*>& final_out_swc, vector<MyMarker*> &final_out_swc_updated)
{
    if(nt.listNeuron[0].pn >0)
    {
        v3d_msg("Please sort the swc file first before using this plugin.",0);
        return false;
    }
    QVector<QVector<V3DLONG> > childs;
    V3DLONG neuronNum = nt.listNeuron.size();
    childs = QVector< QVector<V3DLONG> >(neuronNum, QVector<V3DLONG>() );
    for (V3DLONG i=0;i<neuronNum;i++)
    {
        V3DLONG par = nt.listNeuron[i].pn;
        if (par<0) continue;
        childs[nt.hashNeuron.value(par)].push_back(i);
    }

    for(int j = 0; j < final_out_swc.size(); j++)
    {
        if(final_out_swc[j]->parent != 0)
        {
            int flag_prun = 0;
            int par_x = final_out_swc[j]->parent->x;
            int par_y = final_out_swc[j]->parent->y;
            int par_z = final_out_swc[j]->parent->z;
            int par_r = final_out_swc[j]->parent->radius;

            int dis_prun = sqrt(pow2(final_out_swc[j]->x - par_x) + pow2(final_out_swc[j]->y - par_y) + pow2(final_out_swc[j]->z - par_z));
            if( (final_out_swc[j]->radius + par_r - dis_prun)/dis_prun > 0.3)
            {
                if(childs[j].size() > 0)
                {
                    for(int jj = 0; jj < childs[j].size(); jj++)
                        final_out_swc[childs[j].at(jj)]->parent = final_out_swc[j]->parent;
                }
                flag_prun = 1;
            }

            if(flag_prun == 0)
            {
                final_out_swc_updated.push_back(final_out_swc[j]);
            }
        }
        else
            final_out_swc_updated.push_back(final_out_swc[j]);
    }
    return true;
}

//  reference:  combine_file
QStringList importFileList_addnumbersort(const QString & curFilePath)
{
    QStringList myList;
    myList.clear();
    // get the iamge files namelist in the directory
    QStringList imgSuffix;
    imgSuffix<<"*.swc"<<"*.eswc"<<"*.SWC"<<"*.ESWC";

    QDir dir(curFilePath);
    if(!dir.exists())
    {
        cout <<"Cannot find the directory";
        return myList;
    }
    foreach(QString file, dir.entryList()) // (imgSuffix, QDir::Files, QDir::Name))
    {
        myList += QFileInfo(dir, file).absoluteFilePath();
    }
    //print filenames
    foreach(QString qs, myList) qDebug() << qs;
    return myList;
}

vector<MyMarker*> nt2mm(QList<NeuronSWC> & inswc, QString fileSaveName)
{
    QString tempSaveName = fileSaveName + "temp.swc";
    export_neuronList2file(inswc, tempSaveName);
    vector<MyMarker*> mm_out = readSWC_file(tempSaveName.toStdString());
    //const char * tempRemoveName = tempSaveName.toLatin1().data();
    QFile f;
   // if(remove(tempRemoveName))
    if( !f.remove(tempSaveName))
    {
        cout << "mm_temp file didn't remove."<< endl;
        perror("remove");
    }
    return mm_out;
}

NeuronTree mm2nt(vector<MyMarker*> & inswc, QString fileSaveName)
{
    QString tempSaveName = fileSaveName + "temp.swc";
    saveSWC_file(tempSaveName.toStdString(), inswc);
    NeuronTree nt_out = readSWC_file(tempSaveName);
    //const char * tempRemoveName = tempSaveName.toLatin1().data();
    //const char * tempRemoveName = tempSaveName.toStdString().data();
    //if(remove(tempRemoveName))
    QFile f;
    if(!f.remove(tempSaveName))
    {
        cout << "nt_temp file didn't remove."<< endl;
        perror("remove");
    }
    return nt_out;
}

bool overlap_gold(const V3DPluginArgList & input, V3DPluginArgList & output, V3DPluginCallback2 &callback)
{
    vector<char*>* inlist = (vector<char*>*)(input.at(0).p);
    vector<char*>* outlist = NULL;
    vector<char*>* paralist = NULL;
    if(input.size() != 2)
    {
        printf("need two input. \n");
        return false;
    }
    QString fileOpenName;
    QString fileSaveName;
    char * folder_path;
    QString raw_img;
    if(inlist->size() == 2)
    {
        folder_path = inlist->at(0);
        raw_img = inlist->at(1);
    }
    else
    {
        printf("You need input a folder path and a raw image.");
        return false;
    }

    if (output.size()==1)
    {
        outlist = (vector<char*>*)(output.at(0).p);
        fileSaveName = QString(outlist->at(0));
    }
    else
    {
        printf("You must  specify one  output file.\n");
        return false;
    }
    paralist = (vector<char*>*)(input.at(1).p);
    // method -- 1, 2, 3 or 4
    // dist_para--the distance parameter
    // meth_para--the number of methods whose results are kept
    // prune_thres--prune short trees which lenth are smaller than threshold
    if(paralist->size()!=4)
    {
        printf("Please specify four parameters");
        return false;
    }
    V3DLONG method = atof(paralist->at(0));     // 0 stand for standard method; 1 stand for using mean method.
    double dist_para  = atof(paralist->at(1));
    double meth_para = atof(paralist->at(2));
    double prune_thres = atof(paralist->at(3));
    cout << "dist_para = " << dist_para << endl;
    cout << "meth_para = " << meth_para <<endl;
    cout << "prune_thres = " << prune_thres << endl;
    // read files from a folder
    QStringList swcList = importFileList_addnumbersort(QString(folder_path));
    vector<NeuronTree> nt_list;
    for(V3DLONG i = 2; i < swcList.size(); i++)     // the first two are not files
    {
        QString curPathSWC = swcList.at(i);
        NeuronTree temp = readSWC_file(curPathSWC);
        nt_list.push_back(temp);
    }
    if (nt_list.size()<meth_para) meth_para = nt_list.size();
    cout << "The number of in_swc is " << nt_list.size() << endl;
    if(nt_list.size()==0) return false;

    if(method == 1|| method ==2)
    {
        cout << "nt_list.size = " << nt_list.size() << endl;
        vector<NeuronTree> nt_list_subj;
        for(int i =0; i < nt_list.size(); i++)
        {
            NeuronTree neuron = nt_list[i];
            NeuronTree temp;
            for(int j = 0; j < neuron.listNeuron.size();  j++)
            {
                if(neuron.listNeuron[j].pn != -1)
                {
                    temp.listNeuron.push_back(neuron.listNeuron[j]);
                }
            }
            temp.file = nt_list[i].file;
            //nt_list[i].copy(temp);
            nt_list_subj.push_back(temp);
        }

        QList<NeuronSWC> select_result;
        for(int i = 0; i < nt_list_subj.size(); i++)
        {
            QList<NeuronSWC> neuron1 = nt_list_subj[i].listNeuron;
            int m; // each line in neuron1
            cout << "neuron1.size = " <<neuron1.size() <<endl;
            for(m = 0; m < neuron1.size(); m++)
            {
                int m_count = 1;
                for(int j =0; j < nt_list_subj.size(); j++)
                {
                    if(i != j)
                    {
                        QList<NeuronSWC> neuron2 = nt_list_subj[j].listNeuron;
                        int n; //each line in neuron2
                        double dist;
                        for (n = 0; n < neuron2.size(); n++)
                        {
                            dist = sqrt((neuron1[m].x-neuron2[n].x)*(neuron1[m].x-neuron2[n].x)
                                        +(neuron1[m].y-neuron2[n].y)*(neuron1[m].y-neuron2[n].y)
                                        +(neuron1[m].z-neuron2[n].z)*(neuron1[m].z-neuron2[n].z));
                            if(dist <=  dist_para)  {m_count+=1; break; }
                        }
                    }
                }
                //cout << "m_count = " <<m_count << "    meth para = "<< meth_para<<endl;
                if(m_count >= meth_para)
                {
                    int exist_flag = 0;
                    for (int j = 0;  j < select_result.size(); j++)
                    {
                        if(select_result[j].x == neuron1[m].x && select_result[j].y == neuron1[m].y && select_result[j].z == neuron1[m].z)
                        { exist_flag = 1;break; }
                    }
                    if(exist_flag ==0)
                    {select_result.push_back(neuron1[m]);}
                }
            }
        }
        select_result[0].n = 1;
        for (int i = 0; i < select_result.size(); i++)
        {
            select_result[i].pn = -1;
        }
        // sort_swc begin
        QList<NeuronSWC> sort_result;
        double sort_thres = 10;     // it may should be set from input
        V3DLONG root_id = 1;
        if(!SortSWC(select_result, sort_result, root_id, sort_thres))
        {
            cout << "Error in sorting_swc"<< endl;
            return false;
        }
        //  make radius
        vector<MyMarker*> radius_result = nt2mm(sort_result,fileSaveName);
        if(!evaluate_radius(radius_result, raw_img, callback))
        {
            cout << "Error in evaluate_radius" << endl;
        }
        // prune short trees
        vector<MyMarker*> prune_result;
        prune_short_tree(radius_result,prune_result,prune_thres);
        NeuronTree result = mm2nt(prune_result, fileSaveName);
        /*
         *  This part should make link
         */
        if (!export_neuronList2file(result.listNeuron, fileSaveName))
        {
            printf("fail to write the output swc file.\n");
            return false;
        }

        return true;
    }

    else if(method==3)
    {
        vector<NeuronTree> nt_list_subj;
        cout << "nt_list.size = " << nt_list.size() << endl;
        for(int i =0; i < nt_list.size(); i++)
        {
            NeuronTree neuron = nt_list[i];
            NeuronTree temp;
            // select middle_points of alignment
            for(int j = 1; j < neuron.listNeuron.size();  j+=2)
            {
                neuron.listNeuron[j].x = (neuron.listNeuron[j-1].x +neuron.listNeuron[j].x)/2;
                neuron.listNeuron[j].y = (neuron.listNeuron[j-1].y +neuron.listNeuron[j].y)/2;
                neuron.listNeuron[j].z = (neuron.listNeuron[j-1].z +neuron.listNeuron[j].z)/2;
                temp.listNeuron.push_back(neuron.listNeuron[j]);
            }
            temp.file = nt_list[i].file;
            nt_list_subj.push_back(temp);
            // cout << "i = "<< i <<endl;
        }

        // convert input type
        vector<vector<MyMarker*> > mm_list;
        for(int i = 0; i <nt_list_subj.size(); i++)
        {
            vector<MyMarker*> mm_temp = nt2mm(nt_list_subj[i].listNeuron, fileSaveName);
            mm_list.push_back(mm_temp);
        }

        //  evaluate radius
        cout<<"evaluate radius"<<endl;
        for(int i = 0; i<mm_list.size(); i++)
        {
            if(!evaluate_radius(mm_list[i],raw_img,callback))
            {
                    cout << "Error in evaluate_radius" << endl;
            }
        }

        // convert input type
        vector<NeuronTree> nt_list_r;
        for(int i = 0; i <mm_list.size(); i++)
        {
            NeuronTree nt_temp = mm2nt(mm_list[i], fileSaveName);
            nt_list_r.push_back(nt_temp);
        }

        cout << "method3 begin!"<<endl;
        QList<NeuronSWC>select_result;
        for(int i =0; i <nt_list_r.size();i++)
        {
            QList<NeuronSWC> neuron1 = nt_list_r[i].listNeuron;
            for(int m = 0; m<neuron1.size(); m++)
            {
                int m_count = 1;
                for(int j = 0; j< nt_list_r.size(); j++)
                {
                    if(i != j)
                    {
                        QList<NeuronSWC> neuron2 = nt_list_r[j].listNeuron;
                        double dist;
                        for(int n = 0 ; n<neuron2.size();n++)
                        {
                            dist = sqrt(pow2(neuron1[m].x-neuron2[n].x) + pow2(neuron1[m].y-neuron2[n].y) + pow2(neuron1[m].z-neuron2[n].z));
                            double dist_r = neuron1[m].r + neuron2[n].r;
                            if(dist <= dist_r) { m_count += 1; break;}
                        }
                    }
                }
                if(m_count >= meth_para)
                {
                    int exist_flag = 0;
                    for (int j = 0;  j < select_result.size(); j++)
                    {
                        if(select_result[j].x == neuron1[m].x && select_result[j].y == neuron1[m].y && select_result[j].z == neuron1[m].z)
                        { exist_flag = 1;break; }
                    }
                    if(exist_flag ==0)  select_result.push_back(neuron1[m]);
                }
            }
        }
        select_result[0].n = 1;
        for (int i = 0; i < select_result.size(); i++)
        {
            select_result[i].pn = -1;
        }
        // sort_swc begin
        cout<< "sort_swc begin"<< endl;
        NeuronTree sort_result;
        double sort_thres = 10;
        V3DLONG root_id = 1;
        if(!SortSWC(select_result, sort_result.listNeuron, root_id, sort_thres))
        {
            cout << "Error in sorting_swc"<< endl;
            return false;
        }
        // trim inter node

        vector<MyMarker*> trim_mm = nt2mm(sort_result.listNeuron,fileSaveName);
        NeuronTree trim_nt = mm2nt(trim_mm, fileSaveName);
        vector <MyMarker*> trim_result;
        if(!inter_node_pruning(trim_nt,trim_mm,trim_result))
        {
                printf("inter_node_pruning failed.");
                return false;
        }

        //prune short tree
        cout <<  "prune begin" <<endl;
       // vector<MyMarker*> prune_inter_result = nt2mm(sort_result.listNeuron,fileSaveName); //sort_result  <---> prune_inter
        vector<MyMarker*> temp_result;
        prune_short_tree(trim_result,temp_result,prune_thres);
        NeuronTree result = mm2nt(temp_result, fileSaveName);
        if (!export_neuronList2file(result.listNeuron, fileSaveName))
        {
            printf("fail to write the output swc file.\n");
            return false;
        }
        return true;
    }

    else if(method ==4)
    {
        // method 4's inputs have already evaluated radius
        vector<NeuronTree> nt_list_r = nt_list;

        cout << "method 4 begin!"<<endl;
        QList<NeuronSWC>select_result;
        for(int i =0; i <nt_list_r.size();i++)
        {
            QList<NeuronSWC> neuron1 = nt_list_r[i].listNeuron;
            for(int m = 0; m<neuron1.size(); m++)
            {
                int m_count = 1;
                for(int j = 0; j< nt_list_r.size(); j++)
                {
                    if(i != j)
                    {
                        QList<NeuronSWC> neuron2 = nt_list_r[j].listNeuron;
                        double dist;
                        for(int n = 0 ; n<neuron2.size();n++)
                        {
                            dist = sqrt(pow2(neuron1[m].x-neuron2[n].x) + pow2(neuron1[m].y-neuron2[n].y) + pow2(neuron1[m].z-neuron2[n].z));
                            double dist_r = neuron1[m].r + neuron2[n].r;
                            if(dist <= dist_r) { m_count += 1; break;}
                        }
                    }
                }
                if(m_count >= meth_para)
                {
                    int exist_flag = 0;
                    for (int j = 0;  j < select_result.size(); j++)
                    {
                        if(select_result[j].x == neuron1[m].x && select_result[j].y == neuron1[m].y && select_result[j].z == neuron1[m].z)
                        { exist_flag = 1;break; }
                    }
                    if(exist_flag ==0)  select_result.push_back(neuron1[m]);
                }
            }
        }
        select_result[0].n = 1;
        for (int i = 0; i < select_result.size(); i++)
        {
            select_result[i].pn = -1;
        }
        // sort_swc begin
        cout<< "sort_swc begin"<< endl;
        NeuronTree sort_result;
        double sort_thres = 10;
        V3DLONG root_id = 1;
        if(!SortSWC(select_result, sort_result.listNeuron, root_id, sort_thres))
        {
            cout << "Error in sorting_swc"<< endl;
            return false;
        }
        // trim inter node

        vector<MyMarker*> trim_mm = nt2mm(sort_result.listNeuron,fileSaveName);
        NeuronTree trim_nt = mm2nt(trim_mm, fileSaveName);
        vector <MyMarker*> trim_result;
        cout<<"trim begin";
        if(!inter_node_pruning(trim_nt,trim_mm,trim_result))
        {
                printf("inter_node_pruning failed.");
                return false;
        }

        //prune short tree
        cout <<  "prune begin" <<endl;
       // vector<MyMarker*> prune_inter_result = nt2mm(sort_result.listNeuron,fileSaveName); //sort_result  <---> prune_inter
        vector<MyMarker*> temp_result;
        prune_short_tree(trim_result,temp_result,prune_thres);
        NeuronTree result = mm2nt(temp_result, fileSaveName);
        if (!export_neuronList2file(result.listNeuron, fileSaveName))
        {
            printf("fail to write the output swc file.\n");
            return false;
        }
        return true;
    }
    else
    {
        printf("The method parameter is wrong, please set it within 1,2,3 or 4" );
        return false;
    }
}
