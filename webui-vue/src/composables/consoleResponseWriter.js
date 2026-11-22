export function createConsoleResponseWriter(terminal, redrawInputLine) {
  let responseStreaming = false
  let responseLineOpen = false
  let responseWrapPending = false
  let writes = Promise.resolve()
  let generation = 0

  function write(text, options) {
    const writeGeneration = generation
    writes = writes.then(async () => {
      if (writeGeneration !== generation) {
        return
      }
      await terminal.clearInputLine()
      if (writeGeneration !== generation) {
        return
      }
      let restoredWrap = false
      if (responseStreaming) {
        await terminal.write('\x1B8')
        if (writeGeneration !== generation) {
          return
        }
        restoredWrap = responseWrapPending
        responseWrapPending = false
        if (restoredWrap) {
          await terminal.write('\r\n')
          if (writeGeneration !== generation) {
            return
          }
        }
        if (!options?.response && responseLineOpen && !restoredWrap) {
          await terminal.write('\r\n')
          if (writeGeneration !== generation) {
            return
          }
        }
      }

      await terminal.write(text)
      if (writeGeneration !== generation) {
        return
      }
      if (options?.response) {
        const visibleText = text.replace(/\x1B\[[0-9;:]*m/g, '')
        if (visibleText) {
          responseLineOpen = !visibleText.endsWith('\n')
        }
        responseStreaming = options.streaming
        if (responseStreaming) {
          const activeTerminal = terminal.terminal
          responseWrapPending = Boolean(
            activeTerminal
            && activeTerminal.buffer.active.cursorX === activeTerminal.cols,
          )
          await terminal.write('\x1B7')
          if (writeGeneration !== generation) {
            return
          }
          if (responseLineOpen) {
            await terminal.write('\r\n')
            if (writeGeneration !== generation) {
              return
            }
          }
          await redrawInputLine()
          return
        }
        responseWrapPending = false
        if (responseLineOpen) {
          await terminal.write('\r\n')
          if (writeGeneration !== generation) {
            return
          }
          responseLineOpen = false
        }
      } else {
        responseStreaming = false
        responseLineOpen = false
        responseWrapPending = false
      }
      await redrawInputLine()
    })
    return writes
  }

  function reset() {
    generation++
    responseStreaming = false
    responseLineOpen = false
    responseWrapPending = false
    writes = Promise.resolve()
  }

  return { write, reset }
}
