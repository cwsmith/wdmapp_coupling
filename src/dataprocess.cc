#include "dataprocess.h"
#include "importpart3mesh.h"
#include "commpart1.h"
#include "sendrecv_impl.h"
#include <cassert>

namespace coupler {

DatasProc3D::DatasProc3D(const Part1ParalPar3D& p1pp3d,
    const Part3Mesh3D &p3m3d,
    bool pproc,
    TestCase test_case,
    bool ypar,
    int nummode)
  : preproc(pproc),
    testcase(test_case),
    yparal(ypar),
    p1(p1pp3d.li0,p1pp3d.lj0,p1pp3d.lk0,
	 p1pp3d.ny0, p1pp3d.npy, p1pp3d.mype_y,p1pp3d.blockcount, 
         p1pp3d.res_fact),
    p3(p3m3d.li0,p3m3d.lj0,p3m3d.blockcount,p3m3d.mylk0)
  {
    init();
    AllocDensityArrays();
    AllocPotentArrays();
    if(testcase==TestCase::t0) {
      TestInitPotentAlongz(p3m3d, p1pp3d.npy, nummode);
    }
  }

void DatasProc3D::init()
{
  if(preproc==true){
    if(yparal==true){
      if(p1.li0%p1.npy==0){
        part1li0=p1.li0/p1.npy;
        part3li0=part1li0;
      } else{
        if(p1.mype_y==p1.npy-1){
          part1li0=p1.li0%p1.npy;
          part3li0=part1li0;
        }  else{
          part1li0=p1.li0%p1.npy;
          part3li0=part1li0;
        }
      }
      part1lj0=2*p1.ny0;
      part3lj0=p1.ny0;   // here may need rethinking.
    } else{
      part1li0=p1.li0;
      part3li0=p1.li0;
      part1lj0=2*p1.ny0;
      part3lj0=p1.ny0;   // here may need rethinking.
    }
  }
  sum=0;
  for(LO i=0;i<p3.li0;i++)  sum+=p3.mylk0[i];
 
  mattoplane=new double***[p3.li0];
  for(LO i=0;i<p3.li0;i++){
    mattoplane[i] = new double**[p3.lj0];
    for(LO j=0;j<p3.lj0;j++){
      mattoplane[i][j]=new double*[p3.lj0];
      for(LO k=0;k<p3.lj0;k++){
        mattoplane[i][j][k]=new double[p3.mylk0[i]];
      }    
    }
  }
}

void DatasProc3D::AllocDensityArrays()
{
  if(yparal==false){
    densin=new CV**[p1.li0];
    for(LO i=0;i<p1.li0;i++){
      densin[i]=new CV*[p1.lj0];
      for(LO j=0;j<p1.lj0;j++)
        densin[i][j]=new CV[p1.lk0];
    }
    densinterpo=new CV**[p1.li0];
    for(LO i=0;i<p1.li0;i++){
      densinterpo[i]=new CV*[p1.lj0];
      for(GO j=0;j<p1.lj0;j++){
        densinterpo[i][j]=new CV[p3.mylk0[i]];
      }
    }

   densintmp=new CV[p1.lj0];
   densouttmp=new double[p1.lj0*2];

   denspart3=new double**[p3.li0];
   for(LO i=0;i<p3.li0;i++){
     denspart3[i]=new double*[p3.lj0];
     for(LO j=0; j<p3.lj0; j++)
        denspart3[i][j]=new double[p3.mylk0[i]];
   }

   densTOpart3=new double**[p3.li0];
   for(LO i=0;i<p3.li0;i++){
     densTOpart3[i]=new double*[p3.lj0];
     for(LO j=0; j<p3.lj0; j++)
        densTOpart3[i][j]=new double[p3.mylk0[i]];
   }

   denssend = new double[p3.blockcount*p3.lj0];
   
 } 
}


void DatasProc3D::AllocPotentArrays()
{ 
  if(yparal==false){
    potentin=new double**[p3.li0];
    for(LO i=0;i<p3.li0;i++){
      potentin[i]=new double*[p3.lj0];
      for(LO j=0;j<p3.lj0;j++)
        potentin[i][j]=new double[p3.mylk0[i]];
    }

    potentintmp=new double[p3.lj0];
    potentouttmp=new CV[p3.lj0/2+1];
 
    potentinterpo=new CV**[p3.li0];
    for(LO i=0;i<p3.li0;i++){
      potentinterpo[i]=new CV*[p3.lj0/2];
      for(LO j=0;j<p3.lj0/2;j++)
        potentinterpo[i][j]=new CV[p3.mylk0[i]];
    }
  
    potentpart1=new CV**[p1.li0];
    for(LO i=0;i<p1.li0;i++){
      potentpart1[i]=new CV*[p1.lj0];
      for(LO j=0;j<p1.lj0;j++){
        potentpart1[i][j]=new CV[p1.lk0];
      }
    }
 
   potentsend = new CV[p1.blockcount*p1.lj0];
  
  }
}

//Distribute the sub global potential  2d array received from part3 and reorder the sub 2darray.  
void DatasProc3D::DistriPotentRecvfromPart3(const Part3Mesh3D& p3m3d, const Part1ParalPar3D& p1pp3d,
     const Array2d<double>* fieldfromXGC)
{ 
  double** tmp;
  tmp = new double*[p3m3d.lj0];
  for(LO j=0;j<p3m3d.lj0;j++){
    tmp[j] = new double[p3m3d.blockcount]; 
  }
  double* array;
  array = fieldfromXGC->data();
  for(LO j=0;j<p3m3d.lj0;j++){
    for(GO i=0;i<p3m3d.blockcount;i++)
      tmp[j][i]=array[j*p3m3d.blockcount+i];
  }
  double** subtmp;
  for(LO j=0;j<p3m3d.lj0;j++){
    GO sumbegin=0;       
    subtmp = new double*[p3m3d.li0];
    LO xl=0;
    for(LO i=0;i<p3m3d.li0;i++){
      xl=p3m3d.li1+i;
      subtmp[i]=new double[p3m3d.versurf[xl]];
      for(LO m=0;m<p3m3d.versurf[xl];m++){
        subtmp[i][m]=tmp[j][sumbegin];
        sumbegin=sumbegin+1; 
      }
      reshuffleforward(subtmp[i],p3m3d.nstart[xl],p3m3d.versurf[xl]);
      for(LO k=0;k<p3m3d.mylk0[i];k++){
        potentin[i][j][k]=subtmp[i][p3m3d.mylk1[i]+k];
      }
      free(subtmp[i]);
    }
//      std::cout<<"sumbegin, p3m3d.blockcount="<<sumbegin<<" "<<p3m3d.blockcount<<'\n';
      assert(sumbegin==p3m3d.blockcount); 
  }
  free(subtmp);
  for(LO j=0;j<p3m3d.lj0;j++)
    free(tmp[j]);
  free(tmp);
 } 

// Assemble the potential sub2d array in each process into a bigger one, which is straightforwardly transferred by
// the adios2 API from coupler to Part1.
void DatasProc3D::AssemPotentSendtoPart1(const Part3Mesh3D &p3m3d, const Part1ParalPar3D& p1pp3d)
{
  LO* recvcount = new LO[p1pp3d.npz];
  LO* rdispls = new LO[p1pp3d.npz];

  MPI_Datatype mpitype = getMpiType(LO());      
  MPI_Allgather(&p1pp3d.lk0,1,mpitype,recvcount,1,mpitype,p1pp3d.comm_z); 
  rdispls[0]=0;
    for(LO i=1;i<p1pp3d.npz;i++){
    rdispls[i]=rdispls[0]+recvcount[i];
  }

  CV* tmp = new CV[p1pp3d.nz0]; 
  CV* blocktmp = new CV[p1pp3d.blockcount];
 
  for(LO j=0;j<p1pp3d.lj0;j++){ 
    for(GO h=0;h<p1pp3d.blockcount;h++){
      blocktmp[h] = CV({0.0,0.0});
    }
    GO sumbegin; 
    for(LO i=0;i<p1pp3d.li0;i++){
      sumbegin=0;
      for(LO h=0;h<i;h++){
        sumbegin+=(GO)p1pp3d.nz0;
      }     
      for(LO h=0;h<p1pp3d.nz0;h++){
        tmp[h]=CV({0.0,0.0});
      } 
      MPI_Allgatherv(potentpart1[i][j],p1pp3d.lk0,MPI_CXX_DOUBLE_COMPLEX,tmp,recvcount,rdispls,
                    MPI_CXX_DOUBLE_COMPLEX,p1pp3d.comm_z);    
      for(LO m=0;m<p1pp3d.nz0;m++){
        blocktmp[sumbegin+m]=tmp[m];
      }      
    }    
    assert(sumbegin+p1pp3d.nz0==p1pp3d.blockcount); 
    mpitype = getMpiType(CV());
    if(p1pp3d.mype_x==0){
      for(GO h=0;h<p1pp3d.blockcount;h++){
        potentsend[j*p1pp3d.blockcount+h] = blocktmp[h]; 	    
      }
    }
  }
  delete[] blocktmp; 
  delete[] tmp,recvcount,rdispls;

}

////Distribute the subglobal density  2d array received from part1 to the processes.
void DatasProc3D::DistriDensiRecvfromPart1(const Part3Mesh3D &p3m3d, const Part1ParalPar3D& p1pp3d,
     const Array2d<CV>* densityfromGENE)
{
  CV** tmp; 
  tmp = new CV*[p1pp3d.lj0];
  for(LO j=0;j<p1pp3d.lj0; j++){
    tmp[j] = new CV[p1pp3d.blockcount];
  }
  CV* array = densityfromGENE->data();
  for(LO j=0;j<p1pp3d.lj0;j++){
    for(GO i=0;i<p1pp3d.blockcount;i++){
      tmp[j][i]=array[j*p1pp3d.blockcount+i];
    }
  }
  CV** blocktmp= new CV*[p1pp3d.li0];
  for(LO i=0;i<p1pp3d.li0;i++)
    blocktmp[i]=new CV[p1pp3d.nz0]; 

  for(LO j=0;j<p1pp3d.lj0;j++){
    for(LO i=0;i<p1pp3d.li0;i++){
      GO sumbegin=0;
      for(LO h=0;h<i;h++){
        sumbegin+=(GO)p1pp3d.nz0;
      }
      for(LO m=0;m<p1pp3d.nz0;m++){
        blocktmp[i][m]=tmp[j][sumbegin+m];
      }
      for(LO k=0;k<p1pp3d.lk0;k++){
        densin[i][j][k]=blocktmp[i][p1pp3d.lk1+k];
      }
     }
   }
   for(LO i=0;i<p1pp3d.li0;i++){
     free(blocktmp[i]);
   }
   free(blocktmp);

   for(LO i=0;i<p1pp3d.lj0;i++){
     free(tmp[i]);
   }  
   free(tmp);
}

// Assemble the density sub2d array in each process into a global one, which is straightforwardly transferred by
// the adios2 API from coupler to Part3.

void DatasProc3D::AssemDensiSendtoPart3(const Part3Mesh3D &p3m3d, const Part1ParalPar3D& p1pp3d)
{
  LO* recvcount = new LO[p1pp3d.npz];
  LO* rdispls = new LO[p1pp3d.npz];
  double* blocktmp = new double[p3m3d.blockcount];

  for(LO j=0;j<p3m3d.lj0;j++){
    LO xl=0;
    for(GO h=0;h<p3m3d.blockcount;h++){
      blocktmp[h] = 0.0;
    }

    for(LO h=0;h<p1pp3d.npz;h++){
      recvcount[h]=0;
      rdispls[h]=0;
    }

    for(LO i=0;i<p1pp3d.li0;i++){
      MPI_Datatype mpitype = getMpiType(LO());      
      MPI_Allgather(&p3m3d.mylk0[i],1,mpitype,recvcount,1,mpitype,p1pp3d.comm_z); 
      rdispls[0]=0;
      for(LO k=1;k<p1pp3d.npz;k++){
	rdispls[k]=rdispls[k-1]+recvcount[k-1];
      }
/*
if(p1pp3d.mype==0) {
for(LO k=0;k<p3m3d.mylk0[i];k++){
std::cout<<"i,j="<<i<<" "<<j<<" "<<"denspart3[i][j][k]="<<denspart3[i][j][k]<<'\n';
}
}
*/
      fprintf(stderr, "%d %s 0.01 p1pp3d.li1 %d i %d\n", p1pp3d.mype, __func__, p1pp3d.li1, i);
      xl=p1pp3d.li1+i;    
      fprintf(stderr, "%d %s 0.02 xl %d\n", p1pp3d.mype, __func__, xl);
      double* tmp = new double[p3m3d.versurf[xl]];
      double* tmp_one;
      tmp_one=denspart3[i][j];
      MPI_Allgatherv(tmp_one,p3m3d.mylk0[i],MPI_DOUBLE,tmp,recvcount,rdispls,
                    MPI_DOUBLE,p1pp3d.comm_z);    
 
     fprintf(stderr, "%d %s 0.03 p3m3d.versurf[xl] %d\n", p1pp3d.mype, __func__, p3m3d.versurf[xl]);
     reshufflebackward(tmp,p3m3d.nstart[xl],p3m3d.versurf[xl]);
      GO sumbegin=0;
      for(LO h=0;h<i;h++){
        sumbegin+=GO(p3m3d.versurf[h+p3m3d.li1]);
      } 
      for(LO m=0;m<p3m3d.versurf[xl];m++){
        blocktmp[sumbegin+m]=tmp[m];
      }    
     if(i==p1pp3d.li0-1){
        assert((sumbegin+(GO)p3m3d.versurf[xl]) == p3m3d.blockcount);
      }

      free(tmp); 
    }
   
    for(GO h=0;h<p3m3d.blockcount;h++){
        denssend[j*p3m3d.blockcount+h] = blocktmp[h];
    } 
  }
  free(recvcount);
  free(rdispls);
  free(blocktmp);
}

//I dont's understand the function of the following matrix.
void DatasProc3D::Initmattoplane(const Part3Mesh3D& p3m3d,const Part1ParalPar3D& p1pp3d)
{
  double y_cut;
  LO tmp_ind;
  LO ind_l_tmp;
  LO ind_h_tmp;
  for(LO i=0;i<p3m3d.li0;i++){
    for(LO k=0;k<p3m3d.mylk0[i];k++){
      for(LO j=0;j<p3m3d.lj0;j++){
        y_cut=p1pp3d.C_y[0]*(p1pp3d.q_prof[i]*p3m3d.pzcoords[i][k]-p1pp3d.phi_cut[j])/p1pp3d.dy;
        y_cut=remainder(remainder(y_cut,double(p1pp3d.y_res))+double(p1pp3d.y_res),double(p1pp3d.y_res));
      
        tmp_ind=LO(y_cut);
        ind_l_tmp=remainder(remainder(tmp_ind,p1pp3d.y_res)+p1pp3d.y_res,p1pp3d.y_res);
        ind_h_tmp=remainder(remainder(tmp_ind+1,p1pp3d.y_res)+p1pp3d.y_res,p1pp3d.y_res);

        mattoplane[i][j][ind_h_tmp][k]=y_cut-double(tmp_ind);
        mattoplane[i][j][ind_l_tmp][k]=1.0-(y_cut-double(tmp_ind));
      }
    }
  }
}

//The function of this routines is not clear so far.
void DatasProc3D::DensityToPart3(const Part3Mesh3D& p3m3d,const Part1ParalPar3D& p1pp3d)
{
  for(LO i=0;i<p3.li0;i++){
    for(LO k=0;k<p3.mylk0[i];k++){
      for(LO j=0;j<p3.lj0;j++){
        double tmp=0.0;
        for(LO l=0;l<p3.lj0;j++){
          tmp+=mattoplane[i][j][l][k]*denspart3[i][l][k];
        }
        densTOpart3[i][j][k]=tmp;
      }
    }
  }
}

void DatasProc3D::TestInitPotentAlongz(const Part3Mesh3D& p3m3d,
    const LO npy, const LO n) {
  if(npy==1){
    LO li0,lj0,lk0;
    li0=p3m3d.li0;
    lj0=p3m3d.lj0;
    double ylen;
    double sum;
    double dy=2.0*cplPI/double(lj0);
    for(LO i=0;i<li0;i++){
      lk0=p3m3d.mylk0[i];
      for(LO k=0;k<lk0;k++){
        ylen=0.0;
        for(LO j=0;j<lj0;j++){
          ylen=double(j)*dy;
          sum=0.0;
          for(LO h=0;h<n;h++){
            sum+=cos(double(h+1)*ylen-cplPI);
          }
          potentin[i][j][k]=sum;
        }
      }
    }
  }
}

DatasProc3D::~DatasProc3D()
{
  FreeFourierPlan3D();
  if(densrecv!=NULL){
    for(LO i=0;i<p1.li0;i++){
 
    } 
 
  }
  if(densin!=NULL) delete[] densin;
  if(densintmp!=NULL) delete[] densintmp;
  if(densouttmp!=NULL) delete[] densouttmp;
  if(densinterpo!=NULL) delete[] densinterpo;
  if(denspart3!=NULL) delete[] denspart3;
  if(potentin!=NULL) delete[] potentin;
  if(potentouttmp!=NULL) delete[] potentouttmp;
  if(potentinterpo!=NULL) delete[] potentinterpo;
  if(potentpart1!=NULL) delete[] potentpart1;       
}


}

