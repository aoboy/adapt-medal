# -*- coding: utf-8 -*-
"""
Created on Mon Nov 24 07:38:11 2014

@author: gonga
"""

import numpy as np  # NumPy (multidimensional arrays, linear algebra, ...)
import scipy as sp  # SciPy (signal and image processing library)

import matplotlib as mpl         # Matplotlib (2D/3D plotting library)
import matplotlib.pyplot as plt  # Matplotlib's pyplot: MATLAB-like syntax
from pylab import *              # Matplotlib's pylab interface
ion()        


#---------------------------------------------------------------------------
def read_file(fname):
    with open(fname,'r') as fr:    
        dt = fr.read().split(',')
        xx =[int(x) for x in dt]
        '''sort all values..'''
        xx.sort()
        
        return xx  
#---------------------------------------------------------------------------
def read_file_id(fname, node_id):
    with open(fname,'r') as fr:    
        dt = fr.read().split('\n')
        
        fr.close()
        return dt         
#---------------------------------------------------------------------------
def parse_data(dt, jj):
        xx0=[]
        xx1=[]
        xx2=[]
        for v in dt:
            if v.find('>:') != -1:
                vdt = v.split('>:')
                pp = vdt[1].split(',')
                
                   
                if int(pp[1]) == jj:
                    xx0.append(int(pp[3]))
                    xx1.append(int(pp[4]))
                    xx2.append(int(pp[5]))
                    #else:
                        #break
        #xx.append((xx1,xx2))
        return (xx0, xx1, xx2)#xx         
#---------------------------------------------------------------------------
def emp_cdf(a):
    a.sort()
    yvals = np.arange(len(a))/float(len(a))
    return (a,yvals)
#---------------------------------------------------------------------------
def plot_fig(fname, lstyle, idx):
    
    a=read_file(fname)
    
    xvals,yvals = emp_cdf(a)
    if idx == 0:
        plt.plot(xvals, yvals, lstyle, lw=2)
    else:
        plt.plot(xvals, yvals, lstyle)

#---------------------------------------------------------------------------
'''NEW for getting data'''
def get_data2plot(fname):
    a=read_file(fname)
    '''COMPUTE CDF'''
    xvals,yvals = emp_cdf(a)
    
    return a, xvals,yvals
#---------------------------------------------------------------------------
def ret_sample(a,b):
    l = int(len(a)/50.)
    x=[]
    y=[]
    for i in range(len(a)):
        if i== 0 or i%l == 0:
            x.append(a[i])
            y.append(b[i])
    x.append(a[-1])
    y.append(b[-1])
    
    #print x, y
    return x,y    
#---------------------------------------------------------------------------
colorVec=[[0.0, 0.0, 0.0],
          [0.25, 0.25, 0.25],
          [0.40, 0.40, 0.40],
          [0.45, 0.45, 0.45],
          [0.60, 0.60, 0.60],
          [0.65, 0.65, 0.65],
          [0.80, 0.80, 0.80],
          [0.85, 0.85, 0.85]
            ]
base_name = './'
basename='/home/gonga/Dropbox/neighbor-discovery/adapt-mecc/data/'

fnames=['k1EpidemicData.csv', 'k2EpidemicData.csv','k3EpidemicData.csv', 
        'k4EpidemicData.csv','k5EpidemicData.csv', 'k8EpidemicData.csv']
lg_names=['K=1', 'K=2','K=3','K=4','K=5','K=8']
lg_style=['r','k-o','r--','k-.']
lg_style=['r-','k-.','b-','m','g-','c-']
idx = 0
ID=27
ALL_VEC=[]
for f in fnames:
    fname = base_name+f
    dt = read_file_id(fname, '')
    x, y, z = parse_data(dt, ID)
    
    ALL_VEC.append((x,y,z))
    print x, y, z


'''PLOT NETWORK SIZE'''

for xyz in ALL_VEC:
    x,y,z = xyz
    plt.plot(z, x, lw=2)
    
    

title('Network Size')
ylabel('Network size')
xlabel('Discovery latency (slots)')

figname=basename+'ID'+str(ID)+'Kx_cooja_epidemic.pdf'
plt.legend(lg_names, loc='lower right')
plt.savefig(figname, format='pdf', dpi=120,bbox_inches='tight')

clf()

'''PLOT NEIGHBORS'''
for xyz in ALL_VEC:
    x,y,z = xyz
    plt.plot(z, y, lw=2)

title('Number of 1 hop neighbors')    
ylabel('Number of 1-hop neighbors')
xlabel('Discovery latency (slots)')

figname=basename+'ID'+str(ID)+'Kx_1hop_cooja_epidemic.pdf'
plt.legend(lg_names, loc='lower right')
plt.savefig(figname, format='pdf', dpi=120,bbox_inches='tight')

clf()

fnames=['h1k1EpidemicData.csv', 'h1k2EpidemicData.csv','h1k3EpidemicData.csv', 
        'h1k4EpidemicData.csv','h1k5EpidemicData.csv', 'h1k8EpidemicData.csv']
lg_names=['K=1', 'K=2','K=3','K=4','K=5','K=8']

ALL_VEC=[]
for f in fnames:
    fname = base_name+f
    dt = read_file_id(fname, '')
    x, y, z = parse_data(dt, ID)
    
    ALL_VEC.append((x,y,z))
    print x, y, z


'''PLOT NETWORK SIZE'''

for xyz in ALL_VEC:
    x,y,z = xyz
    plt.plot(z, x, lw=2)
    
    

title('1-hop discovery no epidemic')
ylabel('Network size')
xlabel('Discovery latency (slots)')

figname=basename+'ID'+str(ID)+'H1Kx_cooja_epidemic.pdf'
plt.legend(lg_names, loc='lower right')
plt.savefig(figname, format='pdf', dpi=120,bbox_inches='tight')

clf()

'''PLOT NEIGHBORS'''
for xyz in ALL_VEC:
    x,y,z = xyz
    plt.plot(z, y, lw=2)

title('1-hop discovery no epidemic')    
ylabel('Number of 1-hop neighbors')
xlabel('Discovery latency (slots)')

figname=basename+'ID'+str(ID)+'H1Kx_1hop_cooja_epidemic.pdf'
plt.legend(lg_names, loc='lower right')
plt.savefig(figname, format='pdf', dpi=120,bbox_inches='tight')

clf()