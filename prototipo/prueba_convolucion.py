import numpy as np
x= np.array([2.0, 4.0, 6.0])
h0= np.array([1.0, 0.0, 0.3])
h1= np.array([1.0, 0.0, 0.0, 0.0, 0.2])
y0= np.convolve(x, h0, mode="full") 
y1= np.convolve(x, h1, mode="full")
print("Señal original", x)
print("Señal con bit 0", y0)
print("Señal con bit 1", y1)
D=8
a=0.4
h= np.zeros(D+1)
h[0]=1
h[D]=a
print(" Nueva respuesta al impilso:", h)
y = np.convolve(x, h, mode="full")
print("Señal con el nuevo eco:", y)

