open System
open System.IO
open System.Diagnostics
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

let loadMatrixFromMtx (path: string) : SparseMatrix<float> =
    let lines = File.ReadLines(path) |> Seq.filter (fun l -> not (l.Trim().StartsWith "%"))
    let headerLine = Seq.head lines
    let headerParts = headerLine.Split([|' '; '\t'|], StringSplitOptions.RemoveEmptyEntries)

    if headerParts.Length < 3 then
        failwith $"Invalid header in {path}: expected at least 3 fields, got {headerParts.Length}"

    let rows : uint64<Matrix.nrows> = uint64 headerParts.[0] * 1UL<Matrix.nrows>
    let cols : uint64<Matrix.ncols> = uint64 headerParts.[1] * 1UL<Matrix.ncols>

    let entries : COOEntry<float> list =
        lines
        |> Seq.skip 1
        |> Seq.map (fun l ->
            let p = l.Split([|' '; '\t'|], StringSplitOptions.RemoveEmptyEntries)
            if p.Length < 3 then None
            else
                let r : uint64<Matrix.rowindex> = (uint64 p.[0] - 1UL) * 1UL<Matrix.rowindex>
                let c : uint64<Matrix.colindex> = (uint64 p.[1] - 1UL) * 1UL<Matrix.colindex>
                let v = float p.[2]
                Some (r, c, v))
        |> Seq.choose id
        |> Seq.toList

    Matrix.fromCoordinateList (Matrix.CoordinateList(rows, cols, entries))

let benchmarkMxmFile (m: SparseMatrix<float>) iterations =
    let m2 = generateRandomSparse (uint64 m.ncols) (uint64 m.nrows) 0.05 42
    let _ = mxm opAdd opMult m m2
    let sw = Stopwatch()
    let mutable total = 0L
    for i in 0 .. iterations - 1 do
        sw.Restart()
        let _ = mxm opAdd opMult m m2
        sw.Stop()
        total <- total + sw.ElapsedTicks
    float total / float Stopwatch.Frequency / float iterations

let benchmarkMxvFile (m: SparseMatrix<float>) iterations =
    let v = generateRandomVector (uint64 m.ncols) 43
    let _ = vxm opAdd opMult v m
    let sw = Stopwatch()
    let mutable total = 0L
    for i in 0 .. iterations - 1 do
        sw.Restart()
        let _ = vxm opAdd opMult v m
        sw.Stop()
        total <- total + sw.ElapsedTicks
    float total / float Stopwatch.Frequency / float iterations

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
    if argv.Length >= 2 && Array.exists ((=) "--mtx") argv then
        let file = argv.[0]
        let op = argv.[1]
        let m : SparseMatrix<float> = loadMatrixFromMtx file
        printfn "Loaded: %s (%dx%d)" file (uint64 m.nrows) (uint64 m.ncols)
        let result =
            match op with
            | "mm" -> benchmarkMxmFile m 5
            | "mv" -> benchmarkMxvFile m 5
            | _ -> 0.0
        printfn "%.6f" result
        0
    elif argv.Length = 3 then
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
    else
        printfn "Usage:"
        printfn "  Synthetic:  dotnet run -- <size> <density> <op>"
        printfn "  Real mtx:   dotnet run -- <file.mtx> <op> --mtx"
        1