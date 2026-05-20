open System
open System.Diagnostics
open Common
open Matrix
open Vector
open LinearAlgebra

let opAdd (a: float option) (b: float option) : float option =
    match a, b with
    | Some x, Some y -> Some(x + y)
    | _ -> None

let opMult (a: float option) (b: float option) : float option =
    match a, b with
    | Some x, Some y -> Some(x * y)
    | _ -> None

let generateRandomSparse (rows: uint64) (cols: uint64) (density: float) (seed: int) =
    let rng = Random(seed)
    let nnz = int64(float(rows * cols) * density)
    let coords =
        [0L..nnz-1L]
        |> List.map (fun _ ->
            (uint64(rng.Next(int rows)) * 1UL<Matrix.rowindex>,
             uint64(rng.Next(int cols)) * 1UL<Matrix.colindex>,
             rng.NextDouble() * 2.0 - 1.0))
    Matrix.fromCoordinateList (Matrix.CoordinateList(rows * 1UL<Matrix.nrows>, cols * 1UL<Matrix.ncols>, coords))

let generateRandomVector (length: uint64) (seed: int) =
    let rng = Random(seed)
    let nnz = int64(float(length) * 0.1)
    let coords =
        [0L..nnz-1L]
        |> List.map (fun _ ->
            (uint64(rng.Next(int length)) * 1UL<Vector.index>, rng.NextDouble() * 2.0 - 1.0))
    Vector.fromCoordinateList (Vector.CoordinateList(length * 1UL<Vector.dataLength>, coords))

let benchmarkMultiply rows cols density iterations =
    let m1 = generateRandomSparse rows cols density 42
    let m2 = generateRandomSparse cols rows density 43
    let _ = mxm opAdd opMult m1 m2
    let sw = Stopwatch()
    let mutable total = 0L
    for i in 0 .. iterations - 1 do
        sw.Restart()
        let _ = mxm opAdd opMult m1 m2
        sw.Stop()
        total <- total + sw.ElapsedTicks
    float total / float Stopwatch.Frequency / float iterations

let benchmarkMatVec rows cols density iterations =
    let m = generateRandomSparse rows cols density 42
    let v = generateRandomVector cols 43
    let _ = vxm opAdd opMult v m
    let sw = Stopwatch()
    let mutable total = 0L
    for i in 0 .. iterations - 1 do
        sw.Restart()
        let _ = vxm opAdd opMult v m
        sw.Stop()
        total <- total + sw.ElapsedTicks
    float total / float Stopwatch.Frequency / float iterations

[<EntryPoint>]
let main argv =
    if argv.Length <> 3 then
        printfn "Usage: dotnet run -- <size> <density> <op>"
        1
    else
        let size = uint64 argv.[0]
        let density = float argv.[1]
        let op = argv.[2]
        let result =
            match op with
            | "mm" -> benchmarkMultiply size size density 5
            | "mv" -> benchmarkMatVec size size density 5
            | _ -> 0.0
        printfn "%.6f" result
        0