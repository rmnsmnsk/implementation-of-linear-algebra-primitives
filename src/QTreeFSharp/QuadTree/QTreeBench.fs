open System
open BenchmarkDotNet.Attributes
open BenchmarkDotNet.Running
open Common
open Matrix
open Vector
open LinearAlgebra

let opAdd (a: float option) (b: float option) : float option =
    match a, b with | Some x, Some y -> Some(x + y) | _ -> None

let opMult (a: float option) (b: float option) : float option =
    match a, b with | Some x, Some y -> Some(x * y) | _ -> None

let generateRandomSparse (rows: uint64) (cols: uint64) (density: float) (seed: int) =
    let rng = Random(seed)
    let nnz = int64(float(rows * cols) * density)
    let coords = [0L..nnz-1L] |> List.map (fun _ ->
        (uint64(rng.Next(int rows)) * 1UL<Matrix.rowindex>,
         uint64(rng.Next(int cols)) * 1UL<Matrix.colindex>,
         rng.NextDouble() * 2.0 - 1.0))
    Matrix.fromCoordinateList (Matrix.CoordinateList(rows * 1UL<Matrix.nrows>, cols * 1UL<Matrix.ncols>, coords))

let generateRandomVector (length: uint64) (seed: int) =
    let rng = Random(seed)
    let nnz = int64(float(length) * 0.1)
    let coords = [0L..nnz-1L] |> List.map (fun _ ->
        (uint64(rng.Next(int length)) * 1UL<Vector.index>, rng.NextDouble() * 2.0 - 1.0))
    Vector.fromCoordinateList (Vector.CoordinateList(length * 1UL<Vector.dataLength>, coords))

[<MemoryDiagnoser>]
[<ShortRunJob>]
type QuadTreeBenchmarks() =
    let mutable m1: SparseMatrix<float> option = None
    let mutable m2: SparseMatrix<float> option = None
    let mutable vector: SparseVector<float> option = None

    [<Params(10, 20)>]
    member val Size = 0 with get, set

    [<Params(0.05)>]
    member val Density = 0.0 with get, set

    [<GlobalSetup>]
    member this.Setup() =
        let size = uint64 this.Size
        m1 <- Some(generateRandomSparse size size this.Density 42)
        m2 <- Some(generateRandomSparse size size this.Density 43)
        vector <- Some(generateRandomVector size 43)

    [<Benchmark>]
    member this.MatrixMultiply() =
        match m1, m2 with
        | Some m1, Some m2 -> let _ = mxm opAdd opMult m1 m2 in ()
        | _ -> failwith "Not initialized"

    [<Benchmark>]
    member this.MatrixVectorMultiply() =
        match m1, vector with
        | Some m, Some v -> let _ = vxm opAdd opMult v m in ()
        | _ -> failwith "Not initialized"

[<EntryPoint>]
let main argv =
    let _ = BenchmarkRunner.Run<QuadTreeBenchmarks>()
    0