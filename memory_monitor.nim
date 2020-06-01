import os, osproc, selectors, strformat, strutils, strscans

when not defined(linux):
  {.error: "This application only supports Linux".}

const
  Ticks = 100
  MiB = 1024

proc main() =
  if paramCount() < 1:
    quit "USAGE: " & getAppFilename() & " program [arg]..."
  let
    child = startProcess(paramStr(1), args = commandLineParams().toOpenArray(1, paramCount() - 1),
                         options = {poUsePath, poParentStreams})
    selector = newSelector[bool]()

  discard selector.registerProcess(child.processId, true)

  var lowest, highest, total, samples: int
  while true:
    var
      rss: int
      skip = false
    for line in lines(&"/proc/{child.processId}/smaps"):
      let splitted = line.splitWhitespace
      if splitted.len >= 5:
        if splitted.len > 5 and splitted[5].startsWith("/"):
          # File backed, skip
          skip = true
        else:
          skip = false
      if not skip:
        if splitted[0] == "Pss:":
          rss += parseInt splitted[1]
    lowest = min(lowest, rss)
    highest = max(highest, rss)
    total += rss
    inc samples
    if selector.select(Ticks).len > 0:
      break

  if (let exitcode = child.waitForExit; exitcode != 0):
    echo "Child process terminated with unclean exit code ", exitcode
    quit exitcode

  echo &"Memory usage statistics ({samples} samples): min: {lowest / MiB} MiB avg: {total / samples / MiB} MiB max: {highest / MiB} MiB"

when isMainModule: main()
