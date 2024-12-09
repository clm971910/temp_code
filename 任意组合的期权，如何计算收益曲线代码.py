import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
import matplotlib.ticker as ticker


price = np.linspace(120, 140, 10000)

def buy_call(strike_price, asset_price, premium, count, base):
    return np.maximum(asset_price - strike_price - premium, -premium) * count * base

def sell_call(strike_price, asset_price, premium, count, base):
    return np.minimum(strike_price + premium - asset_price , premium) * count * base

def buy_put(strike_price, asset_price, premium, count, base):
    return np.maximum(strike_price - premium - asset_price, -premium) * count * base

def sell_put(strike_price, asset_price, premium, count, base):
    return np.minimum(asset_price  + premium - strike_price , premium) * count * base


#  UT
# strike=100
# premium=5
# combined_payoff = buy_call(strike, price, premium)
# combined_payoff = sell_call(strike, price, premium)
# combined_payoff = buy_put(strike, price, premium)
# combined_payoff = sell_put(strike, price, premium)

# call1_strike = 180
# call1_premium  = 10
# call2_strike = 200
# call2_premium  = 5
# combined_payoff = sell_call(call1_strike, price, call1_premium) + buy_call(call2_strike, price, call2_premium)

# put_strike = 450
# put_premium = 47
# call_strike = 450
# call_premium = 25
# combined_payoff = buy_call(call_strike, price, call_premium) + buy_put(put_strike, price, put_premium)

# op_list = [['S', 'C', 120, 1.8],
#  ['B', 'C', 130, 0.93],
#   ['B', 'P', 80, 0.47],
#    ['S', 'P', 85, 0.93]]

#  SPY
# op_list = [ ['S', 'C', 520, 65.68, 9500, 100],
#     ['S', 'C', 520, 69.76, 9500, 100],
#    ['B', 'P', 520, 0.66, 9500, 100],
#     ['S', 'P', 520, 2.43, 9500, 100]]

# VOO
# op_list = [ ['S', 'P', 87, 0.16, 1013, 100],
#     ['B', 'P', 88, 0.29, 1013, 100],
#     ]

# 阿里
# 认为不会涨过XX
op_list = [ ['S', 'C', 130, 1.38, 1, 100],
    ['B', 'C', 135, 1.2, 1, 100],
    ]
# 认为不会跌过XX
# op_list = [ ['S', 'P', 60, 0.88, 1, 100],
#     ['B', 'P', 55, 0.54, 1, 100],
#     ]
combined_payoff = 0
cost=0
for op in op_list:
  if op[0] == 'B' and op[1] == 'C':
    combined_payoff = combined_payoff + buy_call(op[2], price, op[3], op[4], op[5])
    cost=cost + op[3] * op[4] * op[5]
  elif op[0] == 'S' and op[1] == 'C':
    combined_payoff = combined_payoff + sell_call(op[2], price, op[3], op[4], op[5])
    cost=cost - op[3] * op[4] * op[5]
  elif op[0] == 'B' and op[1] == 'P':
    combined_payoff = combined_payoff + buy_put(op[2], price, op[3], op[4], op[5])
    cost=cost + op[3] * op[4] * op[5]
  elif op[0] == 'S' and op[1] == 'P':
    combined_payoff = combined_payoff + sell_put(op[2], price, op[3], op[4], op[5])
    cost=cost - op[3] * op[4] * op[5]


plt.figure(figsize=(20, 8))
plt.plot(price, combined_payoff, label='Combined Payoff')
ax = plt.gca()
ax.xaxis.set_major_locator(ticker.MultipleLocator(20))

plt.title('Option Portfolio Payoff')

for op in op_list:
  txt = ":".join(map(str, op))
  print(txt)

min_payoff = np.min(combined_payoff)
print(":".join(map(str, ["max loss", min_payoff])))

max_payoff = np.max(combined_payoff)
print(":".join(map(str, ["max win", max_payoff])))

print(":".join(map(str, ["combine cost", cost])))

plt.xlabel('Underlying Asset Price')
plt.ylabel('Payoff')
plt.legend()
plt.grid(True)
plt.show()