module Graph.TriangleCount

open Common
open Result

type Error =
    | MXMProblem of LinearAlgebra.Error
    | MaskingProblem of Matrix.Error

let triangle_count (graph: Matrix.SparseMatrix<_>) =
    let graph = Matrix.getLowerTriangle graph

    let op_add o1 o2 =
        match o1, o2 with
        | Some x, Some y -> Some <| x + y
        | Some x, None
        | None, Some x -> Some x
        | None, None -> None

    let op_mult o1 o2 =
        match o1, o2 with
        | Some _, Some _ -> Some 1UL
        | _ -> None

    resultM {
        let! C =
            LinearAlgebra.mxm op_add op_mult graph (Matrix.transpose graph)
            |> Result.mapError MXMProblem

        let! CMasked = Matrix.mask C graph Option.isSome |> Result.mapError MaskingProblem

        return Matrix.foldAssociative op_add None CMasked
    }
