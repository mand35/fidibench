#include <vector>
#include <functional>
#include <algorithm>
#include <numeric>
#include <string>
#include <fstream>
#include <limits>
#include <iostream>
#include <sstream>
#include <cstring>

template <size_t NDIMS>
class Upwind {
public:
  // ctor
  Upwind(const std::vector<double>& velocity, 
         const std::vector<double>& lengths, 
         const std::vector<int>& numCells) {

    this->numCells = numCells;
    this->deltas.resize(NDIMS);
    this->upDirection.resize(NDIMS);
    this->v = velocity;
    this->lengths = lengths;
    this->ntot = 1;
    for (size_t j = 0; j < NDIMS; ++j) {
      this->upDirection[j] = -1;
      if (velocity[j] < 0.) this->upDirection[j] = +1;
      this->deltas[j] = lengths[j] / numCells[j];
      this->ntot *= numCells[j];
    }
    this->dimProd.resize(NDIMS);
    this->dimProd[NDIMS - 1] = 1;
    for (int i = (int) NDIMS - 2; i >= 0; --i) {
        // last index varies fastest
        this->dimProd[i] =  this->dimProd[i + 1] * this->numCells[i + 1];
      }
    this->f.resize(this->ntot, 0.0);
    // initialize lower corner to one
    this->f[0] = 1;

    this->coeff.resize(NDIMS);
    for (size_t j = 0; j < NDIMS; ++j) {
      this->coeff[j] = this->v[j] * this->upDirection[j] / this->deltas[j];
    }
  }

  void advect(double deltaTime) {

    int inds[NDIMS];

    // copy
    std::vector<double> oldF(this->f);

    // OpenACC works with primitive arrays
    double* fPtr = &this->f.front();
    double* fOldPtr = &oldF.front();
    double* coeffPtr = &this->coeff.front();
    int* upDirectionPtr = &this->upDirection.front();
    int* dimProdPtr = &this->dimProd.front();
    int* numCellsPtr = &this->numCells.front();
    int ntot = this->ntot;

#pragma acc parallel loop copy(fPtr[ntot]) \
  copyin(fOldPtr[ntot], coeffPtr[NDIMS], dimProdPtr[NDIMS],	\
  upDirectionPtr[NDIMS], numCellsPtr[NDIMS], inds[NDIMS], deltaTime)
    for (int i = 0; i < ntot; ++i) {

#if (NDIMS > 0)
      inds[0] =  i / dimProdPtr[0] % numCellsPtr[0];
#if (NDIMS > 1)
      inds[1] =  i / dimProdPtr[1] % numCellsPtr[1];
#if (NDIMS > 2)
      inds[2] =  i / dimProdPtr[2] % numCellsPtr[2];
#if (NDIMS > 3)
#error Cannot run in more than 3D
#endif
#endif
#endif
#endif

      for (int j = 0; j < NDIMS; ++j) {

        int oldIndex = inds[j];

        // periodic BCs
        inds[j] += upDirectionPtr[j] + numCellsPtr[j];
        inds[j] %= numCellsPtr[j];

        int upI = 0;
        for (int k = 0; k < NDIMS; ++k) {
          upI += dimProdPtr[k] * inds[k];
        }

        fPtr[i] -= deltaTime * coeffPtr[j] * (fOldPtr[upI] - fOldPtr[i]);

        inds[j] = oldIndex;
      }
    } // acc parallel loop
  }

#include "saveVTK.h"

  double checksum() const {
    return std::accumulate(this->f.begin(), this->f.end(), 0.0, std::plus<double>());
  }

  void print() const {
    for (size_t i = 0; i < this->f.size(); ++i) {
      std::cout << i << " " << this->f[i] << '\n';
    }
  }

private:
  std::vector<double> f;
  std::vector<double> v;
  std::vector<double> lengths;
  std::vector<double> deltas;
  std::vector<double> coeff;
  std::vector<int> upDirection;
  std::vector<int> dimProd;
  std::vector<int> numCells;
  int ntot;
};


int main(int argc, char** argv) {

  const int ndims = 3;

  if (argc < 2) {
    std::cout << "must specify number of cells in each direction.\n";
    return 1;
  }

  int numTimeSteps = 100;
  if (argc > 2) {
    numTimeSteps = atoi(argv[2]);
  }

  bool doVtk = false;
  if (argc > 3 && strcmp(argv[3], "vtk") == 0) {
    doVtk = true;
  }

  // same resolution in each direction
  std::vector<int> numCells(ndims, atoi(argv[1]));
  std::cout << "number of cells: ";
  for (size_t i = 0; i < numCells.size(); ++i) {
    std::cout << ' ' << numCells[i];
  }
  std::cout << '\n';
  std::cout << "number of time steps: " << numTimeSteps << '\n';

  std::vector<double> velocity(numCells.size(), 1.0);
  std::vector<double> lengths(numCells.size(), 1.0);

  // compute dt 
  double courant = 0.1;
  double dt = std::numeric_limits<double>::max();
  for (size_t j = 0; j < velocity.size(); ++j) {
    double dx = lengths[j]/numCells[j];
    double val = courant * dx / velocity[j];
    dt = (val < dt? val: dt);
  }

  Upwind<ndims> up(velocity, lengths, numCells);
  //up.saveVTK("up0.vtk");
  for (int i = 0; i < numTimeSteps; ++i) {
    up.advect(dt);
  }
  std::cout << "check sum: " << up.checksum() << '\n';
  if (doVtk) {
    up.saveVTK("up.vtk");
  }
}
