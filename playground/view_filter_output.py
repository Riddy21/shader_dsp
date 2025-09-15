import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('./playground/filter_output.csv')

plt.figure(figsize=(12, 6))
for col in df.columns[1:]:
    plt.plot(df['Sample'], df[col], label=col, alpha=0.7)
plt.xlabel('Sample Index')
plt.ylabel('Amplitude')
plt.title('Frequency Filter Effect Output Waveform')
plt.legend()
plt.grid(True)
plt.show()