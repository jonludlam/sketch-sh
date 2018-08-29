open Toplevel.Types;

let timeoutSeconds = ref(10);
let toplevelWorker = ref(None);

module MapStr = Belt.MutableMap.String;

type callback =
  | ExecuteCallback(Belt.Result.t(list(blockResult), string) => unit)
    : callback;

let ongoingCallbacks: MapStr.t((Js.Global.timeoutId, callback)) =
  MapStr.make();

let workerListener = event => {
  let {w_id, w_message: message} = event##data;

  switch (ongoingCallbacks->MapStr.get(w_id)) {
  | None => ()
  | Some((timeoutId, callback)) =>
    Js.Global.clearTimeout(timeoutId);
    switch (message) {
    | Ready => () /* Ignore this for now */
    | ExecuteResult(result) =>
      switch (callback) {
      | ExecuteCallback(callback) => callback(result)
      | _ => ()
      }
    };
    ongoingCallbacks->MapStr.remove(w_id);
  };
};

let getWorker = () =>
  switch (toplevelWorker^) {
  | None =>
    let newWorker = Toplevel.make();
    toplevelWorker := Some(newWorker);
    newWorker->Toplevel.Top.onMessageFromWorker(workerListener);
    newWorker;
  | Some(worker) => worker
  };
let terminalWorker = id => {
  ongoingCallbacks->MapStr.remove(id);
  getWorker()->Toplevel.Top.terminate;
  toplevelWorker := None;
};

let run = (payload, callback, timeoutCallback) => {
  let messageId = Utils.generateId();

  getWorker()
  |> Toplevel.Top.postMessageToWorker({t_id: messageId, t_message: payload});

  let timeoutId =
    Js.Global.setTimeout(
      () => {
        terminalWorker(messageId);
        timeoutCallback();
      },
      timeoutSeconds^ * 1000,
    );

  ongoingCallbacks->MapStr.set(messageId, (timeoutId, callback));
};

let execute = (lang, blocks, callback) =>
  run(Execute(lang, blocks), ExecuteCallback(callback), () =>
    callback(Belt.Result.Error("Evaluation timeout"))
  );
