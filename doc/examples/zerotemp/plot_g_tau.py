from h5 import HDFArchive
from triqs.gf import *
from matplotlib import pyplot as plt
from triqs.plot.mpl_interface import oplot

# Read data from archive
ar = HDFArchive('results.h5', 'r')

# Plot input and reconstructed correlator
oplot(ar['g_tau'],     mode='R', linewidth=0.8, label="$g(\\tau)$")
oplot(ar['g_rec_tau'], mode='R', linewidth=0.8, label="$g_\mathrm{rec}(\\tau)$")

plt.xlim((0, 10))
plt.ylabel("$g(\\tau)$")
plt.legend(loc="upper right")
