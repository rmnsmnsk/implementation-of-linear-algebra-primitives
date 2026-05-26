module Graph.Boruvka

open Common
open Result


type Error =
    | EdgesCalculationProblem of LinearAlgebra.Error
    | CEdgesCalculationProblem of Vector.Error
    | IndexCalculationProblem of Vector.Error
    | ScatterProblem of Vector.Error
    | FoldValuesProblem of Vector.Error
    | TreeSelectionProblem of Matrix.Error
    | IndexInnerCalculationProblem of Vector.Error
    | DataForUpgradeParentCalculationProblem of Vector.Error

let mst (graph: Matrix.SparseMatrix<_>) =

    let op_mult (i, x) (row, col, w) = Some(w, row)

    let op_min x y =
        match x, y with
        | Some v, Some u -> Some(min v u)
        | Some v, _ -> Some v
        | None, Some v -> Some v
        | _ -> None

    let fixPoint p =
        let rec inner p iter =
            let p2 = Vector.gather p p
            if p2 = p then p else inner p2 (iter + 1)

        let res = inner p 0
        res

    let treeFilter edges index =
        fun i j g ->
            let i = uint64 i * 1UL<Vector.index>
            let j = uint64 j * 1UL<Vector.index>
            let edge = Vector.unsafeGet edges i
            let idx = Vector.unsafeGet index i

            let result =
                match edge, idx with
                | Some(w, dst), Some idxVal -> g = w && idxVal = i && uint64 dst = uint64 j
                | _ -> false

            result

    let graphFilter parent =
        fun i j ->
            let i = uint64 i * 1UL<Vector.index>
            let j = uint64 j * 1UL<Vector.index>
            let parent_i = Vector.unsafeGet parent i
            let parent_j = Vector.unsafeGet parent j

            match parent_i, parent_j with
            | Some v1, Some v2 when v1 <> v2 -> true
            | _ -> false

    let length = uint64 graph.nrows * 1UL<Vector.dataLength>

    let parent = Vector.init length (fun i -> Some i)

    let rec inner (graph: Matrix.SparseMatrix<_>) (tree: Matrix.SparseMatrix<_>) parent iteration =

        if graph.nvals > 0UL<nvals> then

            // Cheapest outgoing edge for each vertex
            // For each vertex j, find the smallest weight edge (i, j, w)
            // such that i and j are in different components.
            // Because graph contains only cross‑component edges,
            // we simply take the min over all neighbors.
            resultM {
                let! edges =
                    LinearAlgebra.vxmi_values op_min op_mult parent graph
                    |> Result.mapError EdgesCalculationProblem

                // Per‑component cheapest edge
                // For each component, keep the smallest edges among its vertices.
                let! cedges =
                    Vector.scatter (Vector.empty length) edges parent op_min
                    |> Result.mapError CEdgesCalculationProblem

                // Propagate component's cheapest edge to all its vertices
                // Each vertex gets its component's edge
                let t = Vector.gather cedges parent

                // Identify a representative vertex for each component
                // For each vertex, if its own edge is the component's cheapest, mark it.
                let! indexInner =
                    Vector.map2i t edges (fun i t e ->
                        match (t, e) with
                        | Some v1, Some v2 when v1 = v2 -> Some i
                        | _ -> None)
                    |> Result.mapError IndexInnerCalculationProblem
                // Among the marked vertices in a component, keep the smallest index.
                let! index =
                    Vector.scatter (Vector.empty length) indexInner parent op_min
                    |> Result.mapError IndexCalculationProblem
                // now each vertex knows its component's representative
                let index = Vector.gather index parent

                // Add selected edges to the MST tree
                // An edge (i, j, w) is added if vertex i is the representative for its component
                // and (i, j, w) is the cheapest edge of that component.
                let treeFilter = treeFilter edges index

                let! tree =
                    Matrix.map2i tree graph (fun i j t g ->
                        match t, g with
                        | Some t, _ -> Some t
                        | None, Some g when treeFilter i j g -> Some g
                        | _ -> None)
                    |> Result.mapError TreeSelectionProblem

                // Compute new parent assignments (merge components)
                // For each component representative i with cheapest edge (w, j), we want to merge
                // the component of i with the component of j. Choose the smaller root.
                let! data_for_update_parent =
                    Vector.map2i edges index (fun i e idx ->
                        match e, idx with
                        | Some(v, j), Some(_i) when _i = i ->
                            let j = uint64 j * 1UL<Vector.index>
                            let parent_i = Vector.unsafeGet parent i
                            let parent_j = Vector.unsafeGet parent j

                            match parent_i, parent_j with
                            | Some p_i, Some p_j -> if p_i < p_j then Some(j, p_i) else Some(i, p_j)
                            | x -> failwithf "Unreachable: %A" x
                        | _ -> None)
                    |> Result.mapError DataForUpgradeParentCalculationProblem

                // Apply the updates
                let! initial_parent_update =
                    Vector.foldValues
                        data_for_update_parent
                        (fun state (i, v) ->
                            match state with
                            | Ok state ->
                                let updateResult = Vector.update state i (Some v) (fun _ _new -> _new)

                                match updateResult with
                                | Ok u -> Ok u
                                | Error e -> Error e
                            | Error e -> Error e)
                        (Ok parent)
                    |> Result.mapError FoldValuesProblem

                let! parent =
                    Vector.scatter parent initial_parent_update parent op_min
                    |> Result.mapError ScatterProblem

                // Then ensure that all vertices in a merged component point to the same root.
                // This is done by a fixpoint (path compression) that repeatedly gathers parents.
                let parent = fixPoint parent

                // Filter the graph to keep only edges between different components
                let graphFilter = graphFilter parent
                let graph = Matrix.mapi graph (fun i j v -> if graphFilter i j then v else None)

                return! inner graph tree parent (iteration + 1)
            }
        else
            Ok tree

    inner graph (Matrix.empty graph.nrows graph.ncols) parent 0
