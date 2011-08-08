/***************************************************************************
                                re_squared.cpp
                             -------------------
                               W. Michael Brown

  Host code for RE-Squared potential acceleration

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                : Fri May 06 2011
    email                : brownw@ornl.gov
 ***************************************************************************/

#ifdef USE_OPENCL
#include "re_squared_cl.h"
#else
#include "re_squared_ptx.h"
#endif

#include "re_squared.h"
#include <cassert>
using namespace LAMMPS_AL;

#define RESquaredT RESquared<numtyp, acctyp>
extern PairGPUDevice<PRECISION,ACC_PRECISION> pair_gpu_device;

template <class numtyp, class acctyp>
RESquaredT::RESquared() : BaseEllipsoid<numtyp,acctyp>(),
                                  _allocated(false) {
}

template <class numtyp, class acctyp>
RESquaredT::~RESquared() { 
  clear();
}
 
template <class numtyp, class acctyp>
int RESquaredT::bytes_per_atom(const int max_nbors) const {
  return this->bytes_per_atom(max_nbors);
}

template <class numtyp, class acctyp>
int RESquaredT::init(const int ntypes, double **host_shape, double **host_well, 
                     double **host_cutsq, double **host_sigma, 
                     double **host_epsilon, int **h_form, double **host_lj1,
                     double **host_lj2, double **host_lj3, double **host_lj4,
                     double **host_offset, const double *host_special_lj,
                     const int nlocal, const int nall, const int max_nbors,
                     const int maxspecial, const double cell_size,
                     const double gpu_split, FILE *_screen) {
  int success;
  success=this->init_base(nlocal,nall,max_nbors,maxspecial,cell_size,gpu_split,
                          _screen,ntypes,h_form,re_squared,re_squared_lj,true);
  if (success!=0)
    return success;

  // If atom type constants fit in shared memory use fast kernel
  int lj_types=ntypes;
  _shared_types=false;
  int max_shared_types=this->device->max_shared_types();
  if (lj_types<=max_shared_types && this->block_size()>=max_shared_types) {
    lj_types=max_shared_types;
    _shared_types=true;
  }
  _lj_types=lj_types;

  // Allocate a host write buffer for copying type data
  UCL_H_Vec<numtyp> host_write(lj_types*lj_types*32,*(this->ucl_device),
                               UCL_WRITE_OPTIMIZED);

  for (int i=0; i<lj_types*lj_types; i++)
    host_write[i]=0.0;

  sigma_epsilon.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack2(ntypes,lj_types,sigma_epsilon,host_write,
			 host_sigma,host_epsilon);

  this->cut_form.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack2(ntypes,lj_types,this->cut_form,host_write,
			 host_cutsq,h_form);

  lj1.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,lj1,host_write,host_lj1,host_lj2,
			 host_cutsq,h_form);

  lj3.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,lj3,host_write,host_lj3,host_lj4,
		         host_offset);

  dev_error.alloc(1,*(this->ucl_device));
  dev_error.zero();
    
  // Allocate, cast and asynchronous memcpy of constant data
  // Copy data for bonded interactions
  special_lj.alloc(4,*(this->ucl_device),UCL_READ_ONLY);
  host_write[0]=static_cast<numtyp>(host_special_lj[0]);
  host_write[1]=static_cast<numtyp>(host_special_lj[1]);
  host_write[2]=static_cast<numtyp>(host_special_lj[2]);
  host_write[3]=static_cast<numtyp>(host_special_lj[3]);
  ucl_copy(special_lj,host_write,4,false);

  // Copy shape, well, sigma, epsilon, and cutsq onto GPU
  // - cast if necessary
  shape.alloc(ntypes,*(this->ucl_device),UCL_READ_ONLY);
  for (int i=0; i<ntypes; i++) {
    host_write[i*4]=host_shape[i][0];
    host_write[i*4+1]=host_shape[i][1];
    host_write[i*4+2]=host_shape[i][2];
  }
  UCL_H_Vec<numtyp4> view4;
  view4.view((numtyp4*)host_write.begin(),shape.numel(),*(this->ucl_device));
  ucl_copy(shape,view4,false);

  well.alloc(ntypes,*(this->ucl_device),UCL_READ_ONLY);
  for (int i=0; i<ntypes; i++) {
    host_write[i*4]=host_well[i][0];
    host_write[i*4+1]=host_well[i][1];
    host_write[i*4+2]=host_well[i][2];
  }
  view4.view((numtyp4*)host_write.begin(),well.numel(),*(this->ucl_device));
  ucl_copy(well,view4,false);
  
  _allocated=true;
  this->_max_bytes=sigma_epsilon.row_bytes()+this->cut_form.row_bytes()+
                   lj1.row_bytes()+lj3.row_bytes()+special_lj.row_bytes()+
                   shape.row_bytes()+well.row_bytes();

  return 0;
}

template <class numtyp, class acctyp>
void RESquaredT::clear() {
  if (!_allocated)
    return;

  UCL_H_Vec<int> err_flag(1,*(this->ucl_device));
  ucl_copy(err_flag,dev_error,false);
  if (err_flag[0] == 2)
    std::cerr << "BAD MATRIX INVERSION IN FORCE COMPUTATION.\n";  
  err_flag.clear();

  _allocated=false;

  dev_error.clear();
  lj1.clear();
  lj3.clear();
  sigma_epsilon.clear();
  this->cut_form.clear();

  shape.clear();
  well.clear();
  special_lj.clear();
  
  this->clear_base();
}

template <class numtyp, class acctyp>
double RESquaredT::host_memory_usage() const {
  return this->host_memory_usage_base()+sizeof(RESquaredT)+
         4*sizeof(numtyp);
}

// ---------------------------------------------------------------------------
// Calculate energies, forces, and torques
// ---------------------------------------------------------------------------
template <class numtyp, class acctyp>
void RESquaredT::loop(const bool _eflag, const bool _vflag) {
  const int BX=this->block_size();
  int eflag, vflag;
  if (_eflag)
    eflag=1;
  else
    eflag=0;

  if (_vflag)
    vflag=1;
  else
    vflag=0;
  
  int GX, NGX;
  int stride=this->nbor->nbor_pitch();
  int ainum=this->ans->inum();

  if (this->_multiple_forms) {
    if (this->_last_ellipse>0) {
      // ------------ ELLIPSE_ELLIPSE ---------------
      this->time_nbor1.start();
      GX=static_cast<int>(ceil(static_cast<double>(this->_last_ellipse)/
                               (BX/this->_threads_per_atom)));
      NGX=static_cast<int>(ceil(static_cast<double>(this->_last_ellipse)/BX));
      this->pack_nbors(NGX,BX, 0, this->_last_ellipse,ELLIPSE_ELLIPSE,
			                 ELLIPSE_ELLIPSE,_shared_types,_lj_types);
      this->time_nbor1.stop();

      this->time_ellipsoid.start();
      this->k_ellipsoid.set_size(GX,BX);
      this->k_ellipsoid.run(&this->atom->dev_x.begin(),
       &this->atom->dev_quat.begin(), &this->shape.begin(), &this->well.begin(),
       &this->special_lj.begin(), &this->sigma_epsilon.begin(), 
       &this->_lj_types, &this->nbor->dev_nbor.begin(), &stride,
       &this->ans->dev_ans.begin(),&ainum,&this->ans->dev_engv.begin(),
       &this->dev_error.begin(), &eflag, &vflag, &this->_last_ellipse,
       &this->_threads_per_atom);
      this->time_ellipsoid.stop();

      // ------------ ELLIPSE_SPHERE ---------------
      this->time_nbor2.start();
      this->pack_nbors(NGX,BX, 0, this->_last_ellipse,ELLIPSE_SPHERE,
			                 ELLIPSE_SPHERE,_shared_types,_lj_types);
      this->time_nbor2.stop();

      this->time_ellipsoid2.start();
      this->k_ellipsoid_sphere.set_size(GX,BX);
      this->k_ellipsoid_sphere.run(&this->atom->dev_x.begin(),
       &this->atom->dev_quat.begin(), &this->shape.begin(), &this->well.begin(),
       &this->special_lj.begin(), &this->sigma_epsilon.begin(), 
       &this->_lj_types, &this->nbor->dev_nbor.begin(), &stride,
       &this->ans->dev_ans.begin(),&ainum,&this->ans->dev_engv.begin(),
       &this->dev_error.begin(), &eflag, &vflag, &this->_last_ellipse,
       &this->_threads_per_atom);
      this->time_ellipsoid2.stop();

      if (this->_last_ellipse==this->ans->inum()) {
        this->time_nbor3.zero();
        this->time_ellipsoid3.zero();
        this->time_lj.zero();
        return;
      }

      // ------------ SPHERE_ELLIPSE ---------------

      this->time_nbor3.start();
      GX=static_cast<int>(ceil(static_cast<double>(this->ans->inum()-
                               this->_last_ellipse)/
                               (BX/this->_threads_per_atom)));
      NGX=static_cast<int>(ceil(static_cast<double>(this->ans->inum()-
                               this->_last_ellipse)/BX));
      this->pack_nbors(NGX,BX,this->_last_ellipse,this->ans->inum(),
			                 SPHERE_ELLIPSE,SPHERE_ELLIPSE,_shared_types,_lj_types);
      this->time_nbor3.stop();

      this->time_ellipsoid3.start();
      this->k_sphere_ellipsoid.set_size(GX,BX);
      this->k_sphere_ellipsoid.run(&this->atom->dev_x.begin(),
        &this->atom->dev_quat.begin(), &this->shape.begin(), 
        &this->well.begin(), &this->special_lj.begin(), 
        &this->sigma_epsilon.begin(), &this->_lj_types,
        &this->nbor->dev_nbor.begin(), &stride, &this->ans->dev_ans.begin(),
        &this->ans->dev_engv.begin(), &this->dev_error.begin(), &eflag,
        &vflag, &this->_last_ellipse, &ainum, &this->_threads_per_atom);
      this->time_ellipsoid3.stop();
   } else {
      this->ans->dev_ans.zero();
      this->ans->dev_engv.zero();
      this->time_nbor1.zero();
      this->time_ellipsoid.zero();                                 
      this->time_nbor2.zero();
      this->time_ellipsoid2.zero();
      this->time_nbor3.zero();
      this->time_ellipsoid3.zero();
    }
    
    // ------------         LJ      ---------------
    this->time_lj.start();
    if (this->_last_ellipse<this->ans->inum()) {
      if (this->_shared_types) {
        this->k_lj_fast.set_size(GX,BX);
        this->k_lj_fast.run(&this->atom->dev_x.begin(), &this->lj1.begin(),
          &this->lj3.begin(), &this->special_lj.begin(), &stride,
          &this->nbor->dev_packed.begin(), &this->ans->dev_ans.begin(),
          &this->ans->dev_engv.begin(), &this->dev_error.begin(),
          &eflag, &vflag, &this->_last_ellipse, &ainum,
          &this->_threads_per_atom);
      } else {
        this->k_lj.set_size(GX,BX);
        this->k_lj.run(&this->atom->dev_x.begin(), &this->lj1.begin(),
          &this->lj3.begin(), &this->_lj_types, &this->special_lj.begin(),
          &stride, &this->nbor->dev_packed.begin(), &this->ans->dev_ans.begin(),
          &this->ans->dev_engv.begin(), &this->dev_error.begin(), &eflag,
          &vflag, &this->_last_ellipse, &ainum, &this->_threads_per_atom);
      }
    }
    this->time_lj.stop();
  } else {
    GX=static_cast<int>(ceil(static_cast<double>(this->ans->inum())/
                             (BX/this->_threads_per_atom)));
    NGX=static_cast<int>(ceil(static_cast<double>(this->ans->inum())/BX));
    this->time_nbor1.start();
    this->pack_nbors(NGX, BX, 0, this->ans->inum(),SPHERE_SPHERE,
		                 ELLIPSE_ELLIPSE,_shared_types,_lj_types);
    this->time_nbor1.stop();
    this->time_ellipsoid.start(); 
    this->k_ellipsoid.set_size(GX,BX);
    this->k_ellipsoid.run(&this->atom->dev_x.begin(), 
      &this->atom->dev_quat.begin(), &this->shape.begin(), &this->well.begin(), 
      &this->special_lj.begin(), &this->sigma_epsilon.begin(), 
      &this->_lj_types, &this->nbor->dev_nbor.begin(), &stride,
      &this->ans->dev_ans.begin(), &ainum,  &this->ans->dev_engv.begin(),
      &this->dev_error.begin(), &eflag, &vflag, &ainum, 
      &this->_threads_per_atom);
    this->time_ellipsoid.stop();
  }
}

template class RESquared<PRECISION,ACC_PRECISION>;
