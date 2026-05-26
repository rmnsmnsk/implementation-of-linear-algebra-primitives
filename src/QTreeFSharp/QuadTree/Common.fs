module Common

[<Measure>]
type nvals

[<Measure>]
type storageSize

type 'value treeValue =
    | Dummy
    | UserValue of 'value

type BinSearchTree<'value> =
    | Leaf of 'value
    | Node of BinSearchTree<'value> * 'value * BinSearchTree<'value>

let powersOfTwo =
    [ 1UL
      2UL
      4UL
      8UL
      16UL
      32UL
      64UL

      128UL

      256UL
      512UL
      1024UL
      2048UL
      4096UL
      8192UL
      16384UL

      32768UL

      65536UL
      131072UL
      262144UL
      524288UL
      1048576UL
      2097152UL
      4194304UL

      8388608UL

      16777216UL
      33554432UL
      67108864UL
      134217728UL
      268435456UL
      536870912UL
      1073741824UL

      2147483648UL

      4294967296UL
      8589934592UL
      17179869184UL
      34359738368UL
      68719476736UL
      137438953472UL
      274877906944UL

      549755813888UL

      1099511627776UL
      2199023255552UL
      4398046511104UL
      8796093022208UL
      17592186044416UL
      35184372088832UL
      70368744177664UL

      140737488355328UL

      281474976710656UL
      562949953421312UL
      1125899906842624UL
      2251799813685248UL
      4503599627370496UL
      9007199254740992UL
      18014398509481984UL

      36028797018963970UL

      72057594037927940UL
      144115188075855870UL
      288230376151711740UL
      576460752303423500UL
      1152921504606847000UL
      2305843009213694000UL
      4611686018427388000UL

      9223372036854776000UL ]

let treeOfPowersOfTwo =
    BinSearchTree.Node(
        BinSearchTree.Node(
            BinSearchTree.Node(
                BinSearchTree.Node(BinSearchTree.Leaf(1UL), 2UL, BinSearchTree.Leaf(4UL)),
                8UL,
                BinSearchTree.Node(BinSearchTree.Leaf(16UL), 32UL, BinSearchTree.Leaf(64UL))
            ),
            128UL,
            BinSearchTree.Node(
                BinSearchTree.Node(BinSearchTree.Leaf(256UL), 512UL, BinSearchTree.Leaf(1024UL)),
                2048UL,
                BinSearchTree.Node(BinSearchTree.Leaf(4096UL), 8192UL, BinSearchTree.Leaf(16384UL))
            )
        ),
        32768UL,
        BinSearchTree.Node(
            BinSearchTree.Node(
                BinSearchTree.Node(BinSearchTree.Leaf(65536UL), 131072UL, BinSearchTree.Leaf(262144UL)),
                524288UL,
                BinSearchTree.Node(BinSearchTree.Leaf(1048576UL), 2097152UL, BinSearchTree.Leaf(4194304UL))
            ),
            8388608UL,
            BinSearchTree.Node(
                BinSearchTree.Node(BinSearchTree.Leaf(16777216UL), 33554432UL, BinSearchTree.Leaf(67108864UL)),
                134217728UL,
                BinSearchTree.Node(BinSearchTree.Leaf(268435456UL), 536870912UL, BinSearchTree.Leaf(1073741824UL))
            )
        )
    )

let getNearestUpperPowerOfTwo (x: uint64) =
    let MAX = 9223372036854776000UL

    let rec find tree rightBound =
        match tree with
        | BinSearchTree.Node(left, v, right) ->
            if x = v then v
            elif x < v then find left v
            else find right rightBound
        | BinSearchTree.Leaf(v) -> if x <= v then v else rightBound

    if x = MAX then
        MAX
    elif x < MAX then
        find treeOfPowersOfTwo 9223372036854776000UL
    else
        failwithf "Argument is too large. Must be not greater then %A" MAX
