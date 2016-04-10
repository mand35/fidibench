#/usr/bin/env python

import numpy
import copy

class Upwind: 

  def __init__(self, velocity, lengths, numCells):
    self.numCells = numCells
    self.ndims = len(velocity)
    self.deltas = numpy.zeros( (self.ndims,), numpy.float64 )
    self.upDirection = numpy.zeros( (self.ndims,), numpy.float64 )
    self.v = velocity
    self.lengths = lengths
    self.ntot = 1
    for j in range(self.ndims):
      self.upDirection[j] = -1
      if velocity[j] < 0.: self.upDirection[j] = +1
      self.deltas[j] = lengths[j] / numCells[j]
      self.ntot *= numCells[j]

    self.dimProd = numpy.ones( (self.ndims,), numpy.int )
    for i in range(self.ndims - 2, -1, -1):
        # last index varies fastest
        self.dimProd[i] =  self.dimProd[i + 1] * self.numCells[i + 1]

    self.f = numpy.zeros( (self.ntot,), numpy.float64 )
    # initialize lower corner to one
    self.f[0] = 1

  def advect(self, deltaTime):
    oldF = copy.deepcopy(self.f)
    for i in xrange(self.ntot): 
      inds = self.getIndexSet(i)
      for j in range(self.ndims):
        oldIndex = inds[j]
        # periodic BCs
        inds[j] += self.upDirection[j] + self.numCells[j]
        inds[j] %= self.numCells[j]

        upI = self.getFlatIndex(inds)
        self.f[i] -= deltaTime * self.v[j] * self.upDirection[j] * (oldF[upI] - oldF[i])/self.deltas[j]
        inds[j] = oldIndex

  def saveVTK(self, fname):
    f = open(fname, 'w')
    print >> f, "# vtk Data Version 2.0"
    print >> f, "upwind.py"
    print >> f, "ASCII"
    print >> f, "DATASET RECTILINEAR_GRID"
    print >> f, "DIMENSIONS"
    # in VTK the first dimension varies fastest so need 
    # to invert the order of the dimensions
    if self.ndims > 2:
      print >> f, ' %d' % (self.numCells[2] + 1),
    else:
      print >> f, " 1",
    if self.ndims > 1:
      print >> f, ' %d' % (self.numCells[1] + 1),
    else:
      print >> f, " 1",
    print >> f, ' %d\n' % (self.numCells[0] + 1)
    print >> f, "X_COORDINATES "
    if self.ndims > 2:
      print >> f, self.numCells[2] + 1,  " double"
      for i in range(self.numCells[2] + 1):
        print >> f, ' %f' % (0.0 + self.deltas[2] * i)    
    else:
      print >> f, "1 double"
      print >> f, "0.0"
    print >> f, "Y_COORDINATES "
    if self.ndims > 1:
      print >> f, self.numCells[1] + 1,  " double"
      for i in range(self.numCells[1] + 1): 
        print >> f, ' %f' % (0.0 + self.deltas[1] * i)  
    else:
      print >> f, "1 double"
      print >> f, "0.0"
    print >> f, "Z_COORDINATES "
    print >> f, self.numCells[0] + 1, " double"
    for i in range(self.numCells[0] + 1):
      print >> f, ' %f' % (0.0 + self.deltas[0] * i)
    print >> f, "CELL_DATA %d" % self.ntot
    print >> f, "SCALARS f double 1"
    print >> f, "LOOKUP_TABLE default"
    for i in range(self.ntot):
      print >> f, self.f[i]
    f.close()

  def checksum(self):
    return numpy.sum(self.f)

  def printOut(self):
    for i in range(len(self.f)):
      print i, ' ', self.f[i]

  def getIndexSet(self, flatIndex):
    res = numpy.zeros( (self.ndims,), numpy.int )
    for i in range(self.ndims):
      res[i] = flatIndex / self.dimProd[i] % self.numCells[i]
    return res

  def getFlatIndex(self, inds):
    return numpy.dot(self.dimProd, inds)

############################################################################################################
def main():
  import sys

  if len(sys.argv) <= 1:
    print "must specify number of cells in each direction."
    return sys.exit(1)

  ndims = 3
  numCells = [int(sys.argv[1])] * 3
  print "number of cells: ", numCells

  numTimeSteps = 100
  if len(sys.argv) > 2:
    numTimeSteps = int(sys.argv[2])


  velocity = numpy.ones( (ndims,), numpy.float64 )
  lengths = numpy.ones( (ndims,), numpy.float64 )

  # compute dt 
  courant = 0.1
  dt = float('inf')
  for j in range(ndims):
    dx = lengths[j]/ float(numCells[j])
    dt = min(courant * dx / velocity[j], dt)

  up = Upwind(velocity, lengths, numCells)
  #up.saveVTK("up0.vtk")
  for i in range(numTimeSteps):
    up.advect(dt)
    #if i % 10 == 0:
    #up.saveVTK("up" + str(i) + ".vtk")

  #up.printOut()
  print "check sum: ", up.checksum()
  #up.saveVTK("up.vtk")

if __name__ == '__main__': main()