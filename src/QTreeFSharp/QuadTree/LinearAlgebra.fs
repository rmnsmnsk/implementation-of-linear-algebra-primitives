module LinearAlgebra

open Common

type MXMError =
    | InconsistentSizeOfArguments
    | MatrixAdditionProblem of Matrix.Error

type Error =
    | InconsistentStructureOfStorages
    | InconsistentSizeOfArguments
    | VectorAdditionProblem of Vector.Error
    | MXMError of MXMError


let rec multScalar op_add (x: uint64) y =
    if x = 1UL then
        y
    else
        let v = multScalar op_add (x / 2UL) y
        let res = op_add v v
        if x % 2UL = 0UL then res else op_add res v


let vxm op_add op_mult (vector: Vector.SparseVector<'a>) (matrix: Matrix.SparseMatrix<'b>) =

    let rec inner (size: uint64<storageSize>) vector matrix =
        let _do x1 x2 y1 y2 y3 y4 =
            let new_size = size / 2UL

            match (inner new_size x1 y1), (inner new_size x1 y2), (inner new_size x2 y3), (inner new_size x2 y4) with
            | Ok((t1, nvals1)), Ok((t2, nvals2)), Ok((t3, nvals3)), Ok((t4, nvals4)) ->
                let data_length = (uint64 new_size) * 1UL<Vector.dataLength>
                let v1 = Vector.SparseVector(data_length, nvals1, (Vector.Storage(new_size, t1)))
                let v2 = Vector.SparseVector(data_length, nvals2, (Vector.Storage(new_size, t2)))
                let v3 = Vector.SparseVector(data_length, nvals3, (Vector.Storage(new_size, t3)))
                let v4 = Vector.SparseVector(data_length, nvals4, (Vector.Storage(new_size, t4)))

                let vAdd v1 (v2: Vector.SparseVector<_>) =
                    match v2.storage.data with
                    | Vector.Leaf(Dummy) -> Ok(v1)
                    | _ -> Vector.map2 v1 v2 op_add

                let z1 = vAdd v1 v3
                let z2 = vAdd v2 v4

                match (z1, z2) with
                | Ok(v1), Ok(v2) -> Ok((Vector.mkNode v1.storage.data v2.storage.data), v1.nvals + v2.nvals)
                | Error(e), _
                | _, Error(e) -> Error(VectorAdditionProblem(e))

            | Error(e), _, _, _
            | _, Error(e), _, _
            | _, _, Error(e), _
            | _, _, _, Error(e) -> Error(e)

        match (vector, matrix) with
        | Vector.btree.Leaf(UserValue(v1)), Matrix.qtree.Leaf(UserValue(v2)) ->
            let res = multScalar op_add (uint64 size) (op_mult v1 v2)

            let nnz =
                match res with
                | None -> 0UL<nvals>
                | _ -> (uint64 size) * 1UL<nvals>

            Ok(Vector.btree.Leaf(UserValue(res)), nnz)

        | Vector.btree.Leaf(UserValue(_)), Matrix.qtree.Node(y1, y2, y3, y4) -> _do vector vector y1 y2 y3 y4
        | Vector.btree.Node(x1, x2), Matrix.qtree.Leaf(UserValue(_)) -> _do x1 x2 matrix matrix matrix matrix
        | Vector.btree.Node(x1, x2), Matrix.qtree.Node(y1, y2, y3, y4) -> _do x1 x2 y1 y2 y3 y4

        | Vector.btree.Leaf(Dummy), _
        | _, Matrix.qtree.Leaf(Dummy) -> Ok(Vector.btree.Leaf(Dummy), 0UL<nvals>)

    if uint64 vector.length = uint64 matrix.nrows then
        let vector_storage =
            if uint64 vector.storage.size < uint64 matrix.storage.size then
                let rec increaseStorage storage_data (current_size: uint64<storageSize>) bound =
                    if current_size = bound then
                        storage_data
                    else
                        increaseStorage
                            (Vector.btree.Node(storage_data, Vector.btree.Leaf(Dummy)))
                            (current_size * 2UL)
                            bound

                let target_size = matrix.storage.size
                Vector.Storage(target_size, increaseStorage vector.storage.data vector.storage.size target_size)
            else
                vector.storage

        match inner vector_storage.size vector_storage.data matrix.storage.data with
        | Error x -> Error x
        | Ok(storage, nvals) ->
            (Vector.SparseVector(
                (uint64 matrix.ncols) * 1UL<Vector.dataLength>,
                nvals,
                (Vector.Storage(matrix.storage.size, storage))
            ))
            |> Ok
    else
        Error Error.InconsistentSizeOfArguments

let vxmi_values
    (op_add: 'c option -> 'c option -> 'c option)
    (op_mult: uint64<Vector.index> * 'a -> uint64<Matrix.rowindex> * uint64<Matrix.colindex> * 'b -> Option<'c>)
    (vector: Vector.SparseVector<'a>)
    (matrix: Matrix.SparseMatrix<'b>)
    =

    let rec inner
        (size: uint64<storageSize>)
        vector
        (vectorIdx: uint64<Vector.index>)
        matrix
        (rowIdx: uint64<Matrix.rowindex>)
        (colIdx: uint64<Matrix.colindex>)
        =
        let _do x1 x2 y1 y2 y3 y4 =
            let new_size = size / 2UL

            match
                (inner new_size x1 vectorIdx y1 rowIdx colIdx),
                (inner new_size x1 vectorIdx y2 rowIdx (colIdx + (uint64 new_size) * 1UL<Matrix.colindex>)),
                (inner
                    new_size
                    x2
                    (vectorIdx + (uint64 new_size) * 1UL<Vector.index>)
                    y3
                    (rowIdx + (uint64 new_size) * 1UL<Matrix.rowindex>)
                    colIdx),
                (inner
                    new_size
                    x2
                    (vectorIdx + (uint64 new_size) * 1UL<Vector.index>)
                    y4
                    (rowIdx + (uint64 new_size) * 1UL<Matrix.rowindex>)
                    (colIdx + (uint64 new_size) * 1UL<Matrix.colindex>))
            with
            | Ok((t1, nvals1)), Ok((t2, nvals2)), Ok((t3, nvals3)), Ok((t4, nvals4)) ->
                let data_length = (uint64 new_size) * 1UL<Vector.dataLength>
                let v1 = Vector.SparseVector(data_length, nvals1, (Vector.Storage(new_size, t1)))
                let v2 = Vector.SparseVector(data_length, nvals2, (Vector.Storage(new_size, t2)))
                let v3 = Vector.SparseVector(data_length, nvals3, (Vector.Storage(new_size, t3)))
                let v4 = Vector.SparseVector(data_length, nvals4, (Vector.Storage(new_size, t4)))

                let vAdd v1 (v2: Vector.SparseVector<_>) =
                    match v2.storage.data with
                    | Vector.Leaf(Dummy) -> Ok(v1)
                    | _ -> Vector.map2 v1 v2 op_add

                let z1 = vAdd v1 v3
                let z2 = vAdd v2 v4

                match (z1, z2) with
                | Ok(v1), Ok(v2) -> Ok((Vector.mkNode v1.storage.data v2.storage.data), v1.nvals + v2.nvals)
                | Error(e), _
                | _, Error(e) -> Error(VectorAdditionProblem(e))

            | Error(e), _, _, _
            | _, Error(e), _, _
            | _, _, Error(e), _
            | _, _, _, Error(e) -> Error(e)

        match (vector, matrix) with
        | Vector.btree.Leaf(UserValue(Some(v1))), Matrix.qtree.Leaf(UserValue(Some(v2))) ->
            if size = 1UL<storageSize> then
                let res = op_mult (vectorIdx, v1) (rowIdx, colIdx, v2)

                let nnz =
                    match res with
                    | None -> 0UL<nvals>
                    | _ -> 1UL<nvals>

                Ok(Vector.btree.Leaf(UserValue(res)), nnz)
            else
                inner
                    size
                    (Vector.btree.Node(vector, vector))
                    vectorIdx
                    (Matrix.qtree.Node(matrix, matrix, matrix, matrix))
                    rowIdx
                    colIdx

        | Vector.btree.Leaf(UserValue(Some(_))), Matrix.qtree.Node(y1, y2, y3, y4) -> _do vector vector y1 y2 y3 y4
        | Vector.btree.Node(x1, x2), Matrix.qtree.Leaf(UserValue(Some(_))) -> _do x1 x2 matrix matrix matrix matrix
        | Vector.btree.Node(x1, x2), Matrix.qtree.Node(y1, y2, y3, y4) -> _do x1 x2 y1 y2 y3 y4
        | Vector.btree.Leaf(UserValue(None)), _
        | _, Matrix.qtree.Leaf(UserValue(None)) -> Ok(Vector.btree.Leaf(UserValue(None)), 0UL<nvals>)

        | Vector.btree.Leaf(Dummy), _
        | _, Matrix.qtree.Leaf(Dummy) -> Ok(Vector.btree.Leaf(Dummy), 0UL<nvals>)

    if uint64 vector.length = uint64 matrix.nrows then
        let vector_storage =
            if uint64 vector.storage.size < uint64 matrix.storage.size then
                let rec increaseStorage storage_data (current_size: uint64<storageSize>) bound =
                    if current_size = bound then
                        storage_data
                    else
                        increaseStorage
                            (Vector.btree.Node(storage_data, Vector.btree.Leaf(Dummy)))
                            (current_size * 2UL)
                            bound

                let target_size = matrix.storage.size
                Vector.Storage(target_size, increaseStorage vector.storage.data vector.storage.size target_size)
            else
                vector.storage

        match
            inner
                vector_storage.size
                vector_storage.data
                0UL<Vector.index>
                matrix.storage.data
                0UL<Matrix.rowindex>
                0UL<Matrix.colindex>
        with
        | Error x -> Error x
        | Ok(storage, nvals) ->
            (Vector.SparseVector(
                (uint64 matrix.ncols) * 1UL<Vector.dataLength>,
                nvals,
                (Vector.Storage(matrix.storage.size, storage))
            ))
            |> Ok
    else
        Error Error.InconsistentSizeOfArguments


let mxm
    (op_add: 'c option -> 'c option -> 'c option)
    (op_mult: 'a option -> 'b option -> 'c option)
    (m1: Matrix.SparseMatrix<'a>)
    (m2: Matrix.SparseMatrix<'b>)
    =

    let rec shrink tree (size: uint64<storageSize>) =
        match tree with
        | Matrix.qtree.Node(nw, ne, sw, _) when ne = sw && ne = Matrix.qtree.Leaf Dummy -> shrink nw (size / 2UL)
        | _ -> tree, size

    let rec expand tree expandRatio =
        match expandRatio with
        | 1UL<storageSize> -> tree
        | _ ->
            expand
                (Matrix.mkNode tree (Matrix.qtree.Leaf Dummy) (Matrix.qtree.Leaf Dummy) (Matrix.qtree.Leaf Dummy))
                (expandRatio / 2UL)

    let rec multiply (size: uint64<storageSize>) m1 m2 =
        let divided (nw1, ne1, sw1, se1) (nw2, ne2, sw2, se2) =
            let halfSize = size / 2UL

            // Double check this code
            let nw1_x_nw2, ne1_x_sw2, nw1_x_ne2, ne1_x_se2, sw1_x_nw2, se1_x_sw2, sw1_x_ne2, se1_x_se2 =
                (multiply halfSize nw1 nw2),
                (multiply halfSize ne1 sw2),
                (multiply halfSize nw1 ne2),
                (multiply halfSize ne1 se2),
                (multiply halfSize sw1 nw2),
                (multiply halfSize se1 sw2),
                (multiply halfSize sw1 ne2),
                (multiply halfSize se1 se2)

            match nw1_x_nw2, ne1_x_sw2, nw1_x_ne2, ne1_x_se2, sw1_x_nw2, se1_x_sw2, sw1_x_ne2, se1_x_se2 with
            | Ok(tnw1_x_nw2, nvals_nw1_x_nw2),
              Ok(tne1_x_sw2, nvals_ne1_x_sw2),
              Ok(tnw1_x_ne2, nvals_nw1_x_ne2),
              Ok(tne1_x_se2, nvals_ne1_x_se2),
              Ok(tsw1_x_nw2, nvals_sw1_x_nw2),
              Ok(tse1_x_sw2, nvals_se1_x_sw2),
              Ok(tsw1_x_ne2, nvals_sw1_x_ne2),
              Ok(tse1_x_se2, nvals_se1_x_se2) ->
                let nrows = (uint64 halfSize) * 1UL<Matrix.nrows>
                let ncols = (uint64 halfSize) * 1UL<Matrix.ncols>
                let storageSize = halfSize

                let nw1_x_nw2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_nw1_x_nw2, Matrix.Storage(storageSize, tnw1_x_nw2))

                let ne1_x_sw2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_ne1_x_sw2, Matrix.Storage(storageSize, tne1_x_sw2))

                let nw1_x_ne2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_nw1_x_ne2, Matrix.Storage(storageSize, tnw1_x_ne2))

                let ne1_x_se2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_ne1_x_se2, Matrix.Storage(storageSize, tne1_x_se2))

                let sw1_x_nw2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_sw1_x_nw2, Matrix.Storage(storageSize, tsw1_x_nw2))

                let se1_x_sw2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_se1_x_sw2, Matrix.Storage(storageSize, tse1_x_sw2))

                let sw1_x_ne2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_sw1_x_ne2, Matrix.Storage(storageSize, tsw1_x_ne2))

                let se1_x_se2 =
                    Matrix.SparseMatrix(nrows, ncols, nvals_se1_x_se2, Matrix.Storage(storageSize, tse1_x_se2))

                let mAdd m1 (m2: Matrix.SparseMatrix<_>) =
                    match m2.storage.data with
                    | Matrix.qtree.Leaf Dummy -> Ok m1
                    | _ -> Matrix.map2 m1 m2 op_add

                let rnw = mAdd nw1_x_nw2 ne1_x_sw2
                let rne = mAdd nw1_x_ne2 ne1_x_se2
                let rsw = mAdd sw1_x_nw2 se1_x_sw2
                let rse = mAdd sw1_x_ne2 se1_x_se2

                match rnw, rne, rsw, rse with
                | Ok(nw), Ok(ne), Ok(sw), Ok(se) ->
                    Ok(
                        Matrix.mkNode nw.storage.data ne.storage.data sw.storage.data se.storage.data,
                        nw.nvals + ne.nvals + sw.nvals + se.nvals
                    )
                | Error(e), _, _, _
                | _, Error(e), _, _
                | _, _, Error(e), _
                | _, _, _, Error(e) -> Error(Error.MXMError(MXMError.MatrixAdditionProblem(e)))

            | Error(e), _, _, _, _, _, _, _
            | _, Error(e), _, _, _, _, _, _
            | _, _, Error(e), _, _, _, _, _
            | _, _, _, Error(e), _, _, _, _
            | _, _, _, _, Error(e), _, _, _
            | _, _, _, _, _, Error(e), _, _
            | _, _, _, _, _, _, Error(e), _
            | _, _, _, _, _, _, _, Error(e) -> Error(e)

        match m1, m2 with
        | Matrix.qtree.Leaf(UserValue v1), Matrix.qtree.Leaf(UserValue v2) ->
            let res = multScalar op_add (uint64 size) (op_mult v1 v2)

            let nnz =
                match res with
                | None -> 0UL<nvals>
                | _ -> (uint64 <| size * size) * 1UL<nvals>

            Ok(Matrix.qtree.Leaf(UserValue res), nnz)
        | Matrix.qtree.Leaf(UserValue(_)), Matrix.qtree.Node(nw2, ne2, sw2, se2) ->
            divided (m1, m1, m1, m1) (nw2, ne2, sw2, se2)
        | Matrix.qtree.Node(nw1, ne1, sw1, se1), Matrix.qtree.Leaf(UserValue(_)) ->
            divided (nw1, ne1, sw1, se1) (m2, m2, m2, m2)
        | Matrix.qtree.Node(nw1, ne1, sw1, se1), Matrix.qtree.Node(nw2, ne2, sw2, se2) ->
            divided (nw1, ne1, sw1, se1) (nw2, ne2, sw2, se2)
        | Matrix.qtree.Leaf Dummy, _
        | _, Matrix.qtree.Leaf Dummy -> Ok(Matrix.qtree.Leaf Dummy, 0UL<nvals>)

    if uint64 m1.ncols = uint64 m2.nrows then
        let nrows = m1.nrows
        let ncols = m2.ncols
        let storageSize = max m1.storage.size m2.storage.size

        let m1_tree, m2_tree =
            if m1.storage.size < m2.storage.size then
                expand (m1.storage.data) (m2.storage.size / (uint64 m1.storage.size)), m2.storage.data
            elif m1.storage.size > m2.storage.size then
                m1.storage.data, expand (m2.storage.data) (m1.storage.size / (uint64 m2.storage.size))
            else
                m1.storage.data, m2.storage.data

        match multiply storageSize m1_tree m2_tree with
        | Ok(tree, nvals) ->
            // in case the resulting storageSize can be smaller
            // e.g. (2x3) * (3x2) matrices
            let tree, storageSize = shrink tree storageSize
            Ok(Matrix.SparseMatrix(nrows, ncols, nvals, Matrix.Storage(storageSize, tree)))
        | Error(e) -> Error(e)
    else
        Error(Error.MXMError MXMError.InconsistentSizeOfArguments)
