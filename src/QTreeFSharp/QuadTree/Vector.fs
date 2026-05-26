module Vector

open Common

type 'value btree =
    | Leaf of 'value treeValue
    | Node of 'value btree * 'value btree

[<Measure>]
type dataLength

[<Measure>]
type index

[<Struct>]
type Storage<'value> =
    val size: uint64<storageSize>
    val data: btree<'value>
    new(_size, _data) = { size = _size; data = _data }

[<Struct>]
type SparseVector<'value> =
    val length: uint64<dataLength>
    val nvals: uint64<nvals>
    val storage: Storage<Option<'value>>

    new(_length, _nvals, _storage) =
        { length = _length
          nvals = _nvals
          storage = _storage }

type Error =
    | InconsistentStructureOfStorages
    | InconsistentSizeOfArguments


let mkNode t1 t2 =
    match (t1, t2) with
    | Leaf(v1), Leaf(v2) when v1 = v2 -> Leaf(v1)
    | _ -> Node(t1, t2)


type UnaryOp<'a, 'b> =
    | ValuesOnly of ('a -> Option<'b>)
    | ValuesOnlyIndexed of (uint64<index> -> 'a -> Option<'b>)
    | AllCells of (Option<'a> -> Option<'b>)
    | AllCellsIndexed of (uint64<index> -> Option<'a> -> Option<'b>)


type AtLeastOne<'a, 'b> =
    | Both of 'a * 'b
    | Left of 'a
    | Right of 'b


type BinaryOp<'a, 'b, 'c> =
    | ValuesOnly of ('a -> 'b -> Option<'c>)
    | ValuesOnlyIndexed of (uint64<index> -> 'a -> 'b -> Option<'c>)
    | AllCells of (Option<'a> -> Option<'b> -> Option<'c>)
    | AllCellsIndexed of (uint64<index> -> Option<'a> -> Option<'b> -> Option<'c>)
    | AtLeastOneValue of (AtLeastOne<'a, 'b> -> Option<'c>)
    | AtLeastOneValueIndexed of (uint64<index> -> AtLeastOne<'a, 'b> -> Option<'c>)
    | LeftValuesOnly of ('a -> Option<'b> -> Option<'c>)
    | LeftValuesOnlyIndexed of (uint64<index> -> 'a -> Option<'b> -> Option<'c>)


let private mapInner (vector: SparseVector<'a>) (op: UnaryOp<'a, 'b>) : SparseVector<'b> =
    let rec inner
        (pointer: uint64<index>)
        (size: uint64<storageSize>)
        (tree: btree<Option<'a>>)
        : btree<Option<'b>> * uint64<nvals> =
        match tree with
        | Node(x1, x2) ->
            let halfSize = size / 2UL
            let t1, nvals1 = inner pointer halfSize x1
            let t2, nvals2 = inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize x2
            mkNode t1 t2, nvals1 + nvals2
        | Leaf(Dummy) -> Leaf(Dummy), 0UL<nvals>
        | Leaf(UserValue(v)) ->
            match op with
            | UnaryOp.ValuesOnly f ->
                match v with
                | None -> Leaf(UserValue(None)), 0UL<nvals>
                | Some v' ->
                    let res = f v'

                    let nvals =
                        if res.IsSome then
                            (uint64 size) * 1UL<nvals>
                        else
                            0UL<nvals>

                    Leaf(UserValue(res)), nvals
            | UnaryOp.ValuesOnlyIndexed f ->
                match v with
                | None -> Leaf(UserValue(None)), 0UL<nvals>
                | Some v' ->
                    if size = 1UL<storageSize> then
                        let res = f pointer v'
                        let nvals = if res.IsSome then 1UL<nvals> else 0UL<nvals>
                        Leaf(UserValue(res)), nvals
                    else
                        let res = f pointer v'

                        if res.IsNone then
                            Leaf(UserValue(res)), 0UL<nvals>
                        else
                            let halfSize = size / 2UL
                            let t1, nvals1 = inner pointer halfSize (Leaf(UserValue(v)))

                            let t2, nvals2 =
                                inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize (Leaf(UserValue(v)))

                            mkNode t1 t2, nvals1 + nvals2
            | UnaryOp.AllCells f ->
                let res = f v

                let nvals =
                    if res.IsSome then
                        (uint64 size) * 1UL<nvals>
                    else
                        0UL<nvals>

                Leaf(UserValue(res)), nvals
            | UnaryOp.AllCellsIndexed f ->
                if size = 1UL<storageSize> then
                    let res = f pointer v
                    let nvals = if res.IsSome then 1UL<nvals> else 0UL<nvals>
                    Leaf(UserValue(res)), nvals
                else
                    let halfSize = size / 2UL
                    let t1, nvals1 = inner pointer halfSize (Leaf(UserValue(v)))

                    let t2, nvals2 =
                        inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize (Leaf(UserValue(v)))

                    mkNode t1 t2, nvals1 + nvals2

    let storage, nvals = inner 0UL<index> vector.storage.size vector.storage.data
    SparseVector(vector.length, nvals, Storage(vector.storage.size, storage))

let private map2Inner
    (vector1: SparseVector<'a>)
    (vector2: SparseVector<'b>)
    (op: BinaryOp<'a, 'b, 'c>)
    : Result<SparseVector<'c>, Error> =
    let len1 = vector1.length

    let rec inner
        (pointer: uint64<index>)
        (size: uint64<storageSize>)
        (tree1: btree<Option<'a>>)
        (tree2: btree<Option<'b>>)
        : Result<btree<Option<'c>> * uint64<nvals>, Error> =
        match (tree1, tree2) with
        | Node(x1, x2), Node(y1, y2) ->
            let halfSize = size / 2UL

            match inner pointer halfSize x1 y1 with
            | Error e -> Error e
            | Ok(t1, nvals1) ->
                match inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize x2 y2 with
                | Error e -> Error e
                | Ok(t2, nvals2) -> Ok(mkNode t1 t2, nvals1 + nvals2)
        | Node(x1, x2), Leaf(v2) ->
            let halfSize = size / 2UL

            match inner pointer halfSize x1 (Leaf(v2)) with
            | Error e -> Error e
            | Ok(t1, nvals1) ->
                match inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize x2 (Leaf(v2)) with
                | Error e -> Error e
                | Ok(t2, nvals2) -> Ok(mkNode t1 t2, nvals1 + nvals2)
        | Leaf(v1), Node(y1, y2) ->
            let halfSize = size / 2UL

            match inner pointer halfSize (Leaf(v1)) y1 with
            | Error e -> Error e
            | Ok(t1, nvals1) ->
                match inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize (Leaf(v1)) y2 with
                | Error e -> Error e
                | Ok(t2, nvals2) -> Ok(mkNode t1 t2, nvals1 + nvals2)
        | Leaf(Dummy), Leaf(Dummy) -> Ok(Leaf(Dummy), 0UL<nvals>)
        | Leaf(UserValue(v1)), Leaf(UserValue(v2)) ->
            let isIndexedOp =
                match op with
                | BinaryOp.ValuesOnlyIndexed _ -> true
                | BinaryOp.AllCellsIndexed _ -> true
                | BinaryOp.AtLeastOneValueIndexed _ -> true
                | BinaryOp.LeftValuesOnlyIndexed _ -> true
                | _ -> false

            if size > 1UL<storageSize> && isIndexedOp then
                let halfSize = size / 2UL

                match inner pointer halfSize (Leaf(UserValue(v1))) (Leaf(UserValue(v2))) with
                | Error e -> Error e
                | Ok(t1, nvals1) ->
                    match
                        inner
                            (pointer + (uint64 halfSize) * 1UL<index>)
                            halfSize
                            (Leaf(UserValue(v1)))
                            (Leaf(UserValue(v2)))
                    with
                    | Error e -> Error e
                    | Ok(t2, nvals2) -> Ok(mkNode t1 t2, nvals1 + nvals2)
            else
                let res =
                    match op with
                    | BinaryOp.ValuesOnly f ->
                        match v1, v2 with
                        | Some a, Some b -> f a b
                        | _ -> None
                    | BinaryOp.ValuesOnlyIndexed f ->
                        match v1, v2 with
                        | Some a, Some b -> f pointer a b
                        | _ -> None
                    | BinaryOp.AllCells f -> f v1 v2
                    | BinaryOp.AllCellsIndexed f -> f pointer v1 v2
                    | BinaryOp.AtLeastOneValue f ->
                        match v1, v2 with
                        | Some a, Some b -> f (AtLeastOne.Both(a, b))
                        | Some a, None -> f (AtLeastOne.Left a)
                        | None, Some b -> f (AtLeastOne.Right b)
                        | None, None -> None
                    | BinaryOp.AtLeastOneValueIndexed f ->
                        match v1, v2 with
                        | Some a, Some b -> f pointer (AtLeastOne.Both(a, b))
                        | Some a, None -> f pointer (AtLeastOne.Left a)
                        | None, Some b -> f pointer (AtLeastOne.Right b)
                        | None, None -> None
                    | BinaryOp.LeftValuesOnly f ->
                        match v1 with
                        | Some a -> f a v2
                        | None -> None
                    | BinaryOp.LeftValuesOnlyIndexed f ->
                        match v1 with
                        | Some a -> f pointer a v2
                        | None -> None

                let nnz =
                    match res with
                    | Some _ -> (uint64 size) * 1UL<nvals>
                    | None -> 0UL<nvals>

                Ok(Leaf(UserValue(res)), nnz)
        | _ -> Error Error.InconsistentStructureOfStorages

    if len1 = vector2.length then
        inner 0UL<index> vector1.storage.size vector1.storage.data vector2.storage.data
        |> Result.map (fun (storage, nvals) -> SparseVector(len1, nvals, Storage(vector1.storage.size, storage)))
    else
        Error Error.InconsistentSizeOfArguments


[<Struct>]
type CoordinateList<'value> =
    val length: uint64<dataLength>
    val data: (uint64<index> * 'value) list
    new(_length, _data) = { length = _length; data = _data }

let update (vector: SparseVector<_>) i v op =
    let rec inner vector (i: uint64<index>) size =
        match vector with
        | Leaf(UserValue x) ->
            if size = 1UL<storageSize> then
                let res = op x v

                let deltaNNZ =
                    match (res, x) with
                    | Some _, None -> 1L
                    | None, Some _ -> -1L
                    | _ -> 0

                Leaf(UserValue(op x v)), deltaNNZ
            else
                let halfSize = size / 2UL

                if uint64 i < uint64 halfSize then
                    let newVector, deltaNNZ = inner vector i halfSize
                    (mkNode newVector vector), deltaNNZ
                else
                    let newVector, deltaNNZ =
                        inner vector ((uint64 i - uint64 halfSize) * 1UL<index>) halfSize

                    (mkNode vector newVector), deltaNNZ
        | Node(x1, x2) ->
            let halfSize = size / 2UL

            if uint64 i < uint64 halfSize then
                let newVector, deltaNNZ = inner x1 i halfSize
                (mkNode newVector x2), deltaNNZ
            else
                let newVector, deltaNNZ =
                    inner x2 ((uint64 i - uint64 halfSize) * 1UL<index>) halfSize

                (mkNode x1 newVector), deltaNNZ
        | _ -> failwith "Unreachable. But seams that index out of range."

    if uint64 i <= uint64 vector.length then
        let storage, deltaNNZ = inner vector.storage.data i vector.storage.size
        let nvals = uint64 (int64 vector.nvals + deltaNNZ) * 1UL<nvals>
        Ok(SparseVector(vector.length, nvals, Storage(vector.storage.size, storage)))
    else
        Error Error.InconsistentSizeOfArguments

let fromCoordinateList (lst: CoordinateList<'a>) : SparseVector<'a> =
    let length = lst.length
    let nvals = (uint64 <| List.length lst.data) * 1UL<nvals>
    let storageSize = (getNearestUpperPowerOfTwo <| uint64 length) * 1UL<storageSize>

    let rec traverse coordinates pointer size =
        match coordinates with
        | [] when uint64 (pointer + size) < uint64 (length) -> Leaf <| UserValue None, []
        | [] when uint64 pointer >= uint64 length -> Leaf Dummy, []
        | (idx, _) :: _ when idx > pointer + size -> Leaf <| UserValue None, coordinates
        | (idx, value) :: xs when idx = pointer && size = 1UL<index> -> Leaf << UserValue <| Some value, xs
        | _ ->
            let halfSize = size / 2UL

            let left, lCoordinates = traverse coordinates pointer halfSize
            let right, rCoordinates = traverse lCoordinates (pointer + halfSize) halfSize

            mkNode left right, rCoordinates

    let sortedCoordinates = List.sort lst.data

    let tree, _ =
        traverse sortedCoordinates 0UL<index> ((uint64 storageSize) * 1UL<index>)

    SparseVector(length, nvals, Storage(storageSize, tree))

let toCoordinateList (vector: SparseVector<'a>) =
    let length = vector.length

    let rec traverse tree accum (pointer: uint64<index>) (size: uint64<index>) =
        match tree with
        | Leaf Dummy
        | Leaf(UserValue(None)) -> accum
        | Leaf(UserValue(Some value)) ->
            accum
            @ [ for idx in 0UL .. uint64 (size - 1UL<index>) -> (pointer + idx * 1UL<index>, value) ]
        | Node(left, right) ->
            let halfSize = size / 2UL
            let lAccum = traverse left accum pointer halfSize
            let rAccum = traverse right lAccum (pointer + halfSize) halfSize
            rAccum

    let lst =
        traverse vector.storage.data [] 0UL<index> ((uint64 vector.storage.size) * 1UL<index>)

    CoordinateList(length, lst)

let empty length =
    fromCoordinateList (CoordinateList(length, []))

let foldValues (vector: SparseVector<'a>) (f: 'b -> 'a -> 'b) (state: 'b) =
    let rec inner state (size: uint64<storageSize>) vector =
        match vector with
        | Leaf(UserValue(Some v)) ->
            let lst = List.replicate (int size) v
            List.fold f state lst
        | Node(x1, x2) ->
            let halfSize = size / 2UL
            inner (inner state halfSize x1) halfSize x2
        | _ -> state

    inner state vector.storage.size vector.storage.data

let map (vector: SparseVector<'a>) f =
    let rec inner (size: uint64<storageSize>) vector =
        match vector with
        | Node(x1, x2) ->
            let t1, nvals1 = inner (size / 2UL) x1
            let t2, nvals2 = inner (size / 2UL) x2
            (mkNode t1 t2), nvals1 + nvals2
        | Leaf(Dummy) -> Leaf(Dummy), 0UL<nvals>
        | Leaf(UserValue(v)) ->
            let res = f v

            let nnz =
                match res with
                | None -> 0UL<nvals>
                | _ -> (uint64 size) * 1UL<nvals>

            Leaf(UserValue(res)), nnz

    let storage, nvals = inner vector.storage.size vector.storage.data
    SparseVector(vector.length, nvals, (Storage(vector.storage.size, storage)))

let mapi (vector: SparseVector<'a>) f =
    let rec inner (pointer: uint64<index>) (size: uint64<storageSize>) vector =
        match vector with
        | Node(x1, x2) ->
            let halfSize = size / 2UL
            let t1, nvals1 = inner pointer halfSize x1
            let t2, nvals2 = inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize x2
            (mkNode t1 t2), nvals1 + nvals2
        | Leaf(Dummy) -> Leaf(Dummy), 0UL<nvals>
        | Leaf(UserValue(v)) ->
            if size = 1UL<storageSize> then
                let res = f pointer v

                let nnz =
                    match res with
                    | Some _ -> 1UL<nvals>
                    | None -> 0UL<nvals>

                Leaf(UserValue(res)), nnz
            else
                let halfSize = size / 2UL
                let t1, nvals1 = inner pointer halfSize (Leaf(UserValue(v)))

                let t2, nvals2 =
                    inner (pointer + (uint64 halfSize) * 1UL<index>) halfSize (Leaf(UserValue(v)))

                (mkNode t1 t2), nvals1 + nvals2

    let storage, nvals = inner 0UL<index> vector.storage.size vector.storage.data
    SparseVector(vector.length, nvals, (Storage(vector.storage.size, storage)))


let init (length: uint64<dataLength>) (f: uint64<index> -> Option<'a>) : SparseVector<'a> =
    let storageSize = (getNearestUpperPowerOfTwo <| uint64 length) * 1UL<storageSize>

    let rec build (pointer: uint64<index>) (size: uint64<storageSize>) =
        if uint64 pointer >= uint64 length then
            Leaf Dummy, 0UL<nvals>
        elif size = 1UL<storageSize> then
            if uint64 pointer >= uint64 length then
                Leaf Dummy, 0UL<nvals>
            else
                let v = f pointer

                Leaf(UserValue v),
                (match v with
                 | Some _ -> 1UL<nvals>
                 | None -> 0UL<nvals>)
        else
            let halfSize = size / 2UL
            let left, nvals1 = build pointer halfSize
            let newPointer = (uint64 pointer + uint64 halfSize) * 1UL<index>
            let right, nvals2 = build newPointer halfSize
            mkNode left right, nvals1 + nvals2

    let storage, nvals = build 0UL<index> storageSize

    SparseVector(length, nvals, Storage(storageSize, storage))

let map2 (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.AllCells f)

let map2Values (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.ValuesOnly f)

let map2AllCells (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.AllCells f)

let map2AtLeastOne (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.AtLeastOneValue f)

let map2LeftValues (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.LeftValuesOnly f)

let mask (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2 vector1 vector2 (fun v1 v2 -> if f v2 then v1 else None)

let map2i (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.AllCellsIndexed f)

let map2iValues (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.ValuesOnlyIndexed f)

let map2iAllCells (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.AllCellsIndexed f)

let map2iAtLeastOne (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.AtLeastOneValueIndexed f)

let map2iLeftValues (vector1: SparseVector<'a>) (vector2: SparseVector<'b>) f =
    map2Inner vector1 vector2 (BinaryOp.LeftValuesOnlyIndexed f)


/// Returns None if index out of range
let unsafeGet (v: SparseVector<'a>) (index: uint64<index>) =
    let originalIndex = index

    let rec getFromTree (tree: btree<Option<'a>>) (size: uint64<storageSize>) (index: uint64<index>) =
        match tree with
        | Leaf Dummy -> None
        | Leaf(UserValue v) -> v
        | Node(l: Option<'a> btree, r) ->
            let halfSize = size / 2UL

            if uint64 index < uint64 halfSize then
                getFromTree l halfSize index
            else
                getFromTree r halfSize ((uint64 index - uint64 halfSize) * 1UL<index>)

    getFromTree v.storage.data v.storage.size index

/// Gather: w[i] = v[idx[i]]
let gather (v: SparseVector<'value>) (idx: SparseVector<uint64<index>>) : SparseVector<'value> =
    map idx (fun i ->
        match i with
        | Some i -> unsafeGet v i
        | None -> None)

let mergeSort (v: SparseVector<'a>) (compare: Option<'a> -> Option<'a> -> int) : SparseVector<'a> =
    let storageSize = v.storage.size
    let nvals = v.nvals

    // Extract values from tree
    let rec extract tree =
        match tree with
        | Leaf Dummy -> []
        | Leaf(UserValue None) -> []
        | Leaf(UserValue v) -> [ v ]
        | Node(l, r) -> extract l @ extract r

    // Place sorted values into original tree structure
    let rec place tree sortedVals =
        match tree, sortedVals with
        | Leaf Dummy, rest -> Leaf Dummy, rest
        | Leaf(UserValue None), rest -> Leaf(UserValue None), rest
        | Leaf(UserValue _), v :: rest -> Leaf(UserValue v), rest
        | Leaf(UserValue _), [] -> Leaf Dummy, []
        | Node(l, r), vals ->
            let l', r1 = place l vals
            let r', r2 = place r r1
            mkNode l' r', r2

    let values = extract v.storage.data
    let sortedVals = List.sortWith (fun a b -> compare a b) values
    let newTree, _ = place v.storage.data sortedVals

    SparseVector(v.length, nvals, Storage(storageSize, newTree))

/// Scatter: w[idx[i]] = op(w[idx[i]], v[i])
let scatter
    (w: SparseVector<'value>)
    (v: SparseVector<'value>)
    (idx: SparseVector<uint64<index>>)
    (op: Option<'value> -> Option<'value> -> Option<'value>)
    =
    let pairsVec =
        map2 idx v (fun i v ->
            match i, v with
            | Some i, Some v -> Some(i, v)
            | _ -> None)

    match pairsVec with
    | Ok pv ->
        foldValues
            pv
            (fun state (idx, v) ->
                match state with
                | Ok state -> update state idx (Some v) op
                | Error x -> Error x)
            (Ok w)
    | Error x -> Error Error.InconsistentStructureOfStorages
