import matplotlib.pyplot as plt
import math
import numpy as np
import pandas as pd
import noise

from .item import RegenerationFunction

def zero_regeneration_fn(pos, t, *args):
    return 0.0

def constant_regeneration_fn(pos, t, *args):
    return args[0]


# Args

REPETITIONS = 7

UMAX = 1
PERIOD = 6
SMOOTHING = 4

OCTAVES = 7
PERSISTENCE = 2
FREQUENCY = 5
EXTENSION = 8


def Carre(nmax):
    #rectangular signal
    omega = 2*np.pi/PERIOD
    t = np.linspace(0, REPETITIONS*PERIOD/2, 40*REPETITIONS)
    tmp_carre = np.zeros(40*REPETITIONS)
    for n in range(0, nmax):
        tmp_carre = tmp_carre+np.sin((2*n + 1) * omega*t)/(2 * n + 1)
    scale_carre = [4*UMAX/np.pi]*40*REPETITIONS
    tmp_carre = scale_carre*tmp_carre
    
    #smoothing
    df = pd.DataFrame(list(zip(t, tmp_carre)),
               columns =['Time', 'Value'])
    df['Smooth'] = pd.Series.rolling(df.Value, window=SMOOTHING).mean()
    df['Noisy'] = df.Smooth

    #noise
    df['Noisy'] = df['Smooth'].copy()
    pic = [2.5*noise.pnoise1(i/(40*REPETITIONS),octaves=OCTAVES, persistence=PERSISTENCE, lacunarity=FREQUENCY, repeat=1024, base=1) for i in range(40*REPETITIONS)]
    for i in range(1,REPETITIONS):
        for j in range(-EXTENSION, EXTENSION):
            df.Noisy[40*i+j] = pic[40*i+j]
    #beginning
    for j in range (EXTENSION):
        df.Noisy[j] = pic[j]
    #end
    for j in range (-EXTENSION,1):
        df.Noisy[j] = pic[j]



    plt.plot(df.Time, df.Noisy)


plt.figure()
Carre(30)
plt.show()