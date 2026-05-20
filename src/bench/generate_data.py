import json
import random
import os

os.makedirs("data", exist_ok=True)

configs = [
    (100, 0.05),
    (500, 0.02),
    (1000, 0.01),
    (2000, 0.005),
]

for size, density in configs:
    random.seed(42)
    nnz = int(size * size * density)
    row_idx = [random.randint(0, size - 1) for _ in range(nnz)]
    col_idx = [random.randint(0, size - 1) for _ in range(nnz)]
    values = [random.gauss(0, 1) for _ in range(nnz)]

    data = {
        "size": size,
        "density": density,
        "row_indices": row_idx,
        "col_indices": col_idx,
        "values": values
    }

    with open(f"data/matrix_{size}_{density}.json", "w") as f:
        json.dump(data, f)

    print(f"Generated {size}x{size}, density={density}, nnz={nnz}")