# Water Pump Calibration

The firmware converts a requested water volume in mL to a pump runtime in milliseconds with this formula:

```text
runtime_ms = floor((mL * 1000) / 39) + compensation_ms
```

The compensation comes from the lookup table below. The `max_mL` column means the entry applies to all requests up to and including that volume.

| max mL    | compensation ms |
| --------- | --------------: |
| 25        |             500 |
| 50        |             300 |
| 75        |             100 |
| 100       |            -100 |
| 125       |            -300 |
| 150       |            -600 |
| 175       |            -800 |
| 200       |           -1000 |
| 225       |           -1250 |
| 250       |           -1350 |
| 275       |           -1600 |
| 300       |           -1800 |
| 330       |           -2000 |
| 350       |           -2200 |
| 375       |           -2500 |
| 400       |           -2700 |
| 430       |           -2800 |
| 460       |           -3000 |
| above 460 |           -3700 |

This table is the calibration source used by `lib/drivers/wpump_converter.c`.
