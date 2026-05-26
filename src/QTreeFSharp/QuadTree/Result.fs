module Result

module Error =
    let map (f: 'a -> 'b) (err: 'a) : 'b = f err

let mapError (f: 'e1 -> 'e2) (result: Result<'ok, 'e1>) : Result<'ok, 'e2> =
    match result with
    | Ok ok -> Ok ok
    | Error err -> Error(f err)

type ResultBuilder() =
    member _.Return(x: 'ok) : Result<'ok, 'err> = Ok x
    member _.ReturnFrom(result: Result<'ok, 'err>) : Result<'ok, 'err> = result

    member _.Bind(result: Result<'ok, 'err>, f: 'ok -> Result<'ok2, 'err>) : Result<'ok2, 'err> =
        match result with
        | Ok ok -> f ok
        | Error err -> Error err

    member _.Zero() : Result<unit, 'err> = Ok()
    member _.Delay(f: unit -> Result<'ok, 'err>) = f
    member _.Run(f: unit -> Result<'ok, 'err>) = f ()

    member this.While(guard: unit -> bool, body: unit -> Result<unit, 'err>) =
        if guard () then
            this.Bind(body (), fun () -> this.While(guard, body))
        else
            this.Zero()

    member this.For(sequence: seq<'ok>, f: 'ok -> Result<unit, 'err>) =
        use en = sequence.GetEnumerator()
        this.While(en.MoveNext, fun () -> f en.Current)

    member _.Combine(result1: Result<unit, 'err>, result2: Result<'ok, 'err>) : Result<'ok, 'err> =
        match result1 with
        | Ok() -> result2
        | Error err -> Error err

    member this.MergeSources(result1: Result<'ok1, 'err>, result2: Result<'ok2, 'err>) : Result<'ok1 * 'ok2, 'err> =
        match result1, result2 with
        | Ok ok1, Ok ok2 -> Ok(ok1, ok2)
        | Error err, _
        | _, Error err -> Error err

let resultM = ResultBuilder()
