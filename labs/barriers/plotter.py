P = [1,2,4,8,16] 
T1 = [6.18532e-02, 3.17831e-02, 1.70200e-02, 9.57894e-03, 4.59099e-03]
N = [1280000, 2560000, 5120000, 10240000, 20480000, 40960000]

T2 = [2.53701e-03, 5.39708e-03, 9.80091e-03, 1.82111e-02, 3.56920e-02, 7.06e-02]

T3 = [
1.31083e-03,
2.57799e-03,
4.72811e-03,
9.48360e-03,
1.89359e-02,
3.68e-02]

OMP_THR1 = [2,4,6,8,10,12,14,16]
OMP_THR1 = [2,4,6,8,10,12,14]

OMP_CEN = [557.000000, 937.250000, 1285.333333, 1614.750000, 1864.900000, 2193.583333, 2500.857143, 2662.875000]
OMP_CEN = [557.000000, 937.250000, 1285.333333, 1614.750000, 1864.900000, 2193.583333, 2500.857143]
OMP_TOR = [540.500000, 887.250000, 949.333333, 1011.875000, 1064.700000, 1246.750000, 1368.214286, 1405.687500]
OMP_TOR = [540.500000, 887.250000, 949.333333, 1011.875000, 1064.700000, 1246.750000, 1368.214286]

MPI_NODE = [2, 4, 6, 8, 10, 12]
MPI_DIS = [21165.490150, 46698.331833, 58936.198552, 67263.245583, 78298.187256, 75294.375420]
MPI_TOUR = [42320.370674, 55206.239223, 66723.267237, 89011.639357, 81549.906731, 88825.960954]

OMP_THR = [2, 4, 6, 8, 10, 12]
OMP_MPI_2 = [20326211.571693, 44609775.483608, 57490276.018778, 110186787.545681, 138492497.301102, 170961873.064438]
OMP_MPI_4 = [23717862.159014, 55314155.593514, 69066070.536772, 128028020.404279, 159606819.093227, 199362257.406116]
OMP_MPI_6 = [29379681.011041, 64963460.405668, 81169171.313445, 141485055.853923, 181421315.403779, 223551113.996241]
OMP_MPI_8 = [31642328.247428, 64562684.752047, 82319391.921163, 145172102.142125, 183862309.589982, 225799933.401247]
# Y_2_8_OMP_MPI_2 = [20521069.049835, 44913320.600986, 57890601.336956, 109694847.866893, 138803821.420670, 169705896.615982]
# Y_2_8_OMP_MPI_4 = []
# Y_2_8_OMP_MPI_6 = []
# Y_2_8_OMP_MPI_8 = []

import matplotlib.pyplot as plt 
import numpy as np

fig, ax = plt.subplots() 
ax.plot(OMP_THR1, OMP_CEN, marker='.', linestyle='-', label='Centralized Barrier')
ax.plot(OMP_THR1, OMP_TOR, marker = '.', linestyle = '-', label='Tournament Barrier')
ax.set(xlabel='#Threads (T)', ylabel='Time (ns)', title='Time vs # of OpenMP Threads')
leg1 = ax.legend(loc='upper left')
plt.show() 


fig, ax = plt.subplots() 
ax.plot(MPI_NODE, MPI_DIS, marker='.', linestyle='-', label='Dissemination Barrier')
ax.plot(MPI_NODE, MPI_TOUR, marker = '.', linestyle = '-', label='Tournament Barrier')
ax.set(xlabel='#Processes (P)', ylabel='Time (ns)', title='Time vs # of MPI Processes')
leg1 = ax.legend(loc='upper left')
plt.show() 


fig, ax = plt.subplots() 
ax.plot(OMP_THR, OMP_MPI_2, marker = '.', linestyle = '-', label='Processors 2')
ax.plot(OMP_THR, OMP_MPI_4, marker = '.', linestyle = '-', label='Processors 4')
ax.plot(OMP_THR, OMP_MPI_6, marker = '.', linestyle = '-', label='Processors 6')
ax.plot(OMP_THR, OMP_MPI_8, marker = '.', linestyle = '-', label='Processors 8')
ax.set(xlabel='#Threads (T)', ylabel='Time (ns)', title='Time vs Threads with varying processors')
leg1 = ax.legend(loc='upper left')
plt.show()


fig, ax = plt.subplots() 
ax.plot(N, T2, marker = '.', linestyle = '-')
ax.set(xlabel='#Problem Size (N)', ylabel='Time (s)', title='N vs T [P = 8, C = 100]')
plt.show() 

#fig, ax = plt.subplots() 
#ax.plot(np.log2(N), T2, marker = '.', linestyle = '-')
#ax.set(xlabel='log of #Problem Size (N)', ylabel='Time (s)', label='log(N) vs T [P = 8, C = 100]')
#plt.show() 

fig, ax = plt.subplots() 
ax.plot(np.log2(N), T2, marker = '.', linestyle = '-', label='log(N) vs T [P = 8, C = 100]')
ax.plot(np.log2(N), T3, marker = '.', linestyle = '-', label='log(N) vs T [P = 16, C = 100]')
ax.set(xlabel='log of #Problem Size (N)', ylabel='Time (s)', title='log(N) vs T')
leg1 = ax.legend(loc='upper left')
plt.show() 
