# NEUROMEKA

Modbus TCP Communication

## Control Var

| Function         | Variable |
|------------------|----------|
| Spinner          | 950      |
| Mixer            | 951      |
| Product Finished | 952      |
| **Reserved**     | 953      |
| **Reserved**     | 954      |

#### Values

| Value | Function |
|-------|----------|
| 0     | OFF      |
| 1     | ON       |  

## State Var

| Input            | Variable |
|------------------|----------|
| Level            | 955      |
| Pressure         | 956      |
| Temperature      | 957      |
| Compressor       | 958      |
| Scale Connection | 959      |


#### Values

| Value | Function |
|-------|----------|
| 0     | OFF      |
| 1     | ON       |

# KUKA

KukaVar Proxy Communication

## Variables

| Function         | Variable         |
|------------------|------------------|
| Spinner          | ELECTROFREEZE[0] |
| Mixer            | ELECTROFREEZE[1] |
| Product Finished | ELECTROFREEZE[2] |
| **Reserved**     | ELECTROFREEZE[3] |
| **Reserved**     | ELECTROFREEZE[4] |

| Input            | Variable         |
|------------------|------------------|
| Level            | ELECTROFREEZE[5] |
| Pressure         | ELECTROFREEZE[6] |
| Temperature      | ELECTROFREEZE[7] |
| Compressor       | ELECTROFREEZE[8] |
| Scale Connection | ELECTROFREEZE[9] |



#### Values

| Value | Function |
|-------|----------|
| 0     | OFF      |
| 1     | ON       |