export function createConsoleResponseWriter(terminal, redrawInputLine) {
  let responseStreaming = false
  let responseLineOpen = false
  let responseWrapPending = false
  let writes = Promise.resolve()
  let generation = 0

  function write(text, options) {
    const writeGeneration = generation
    writes = writes.then(() => terminal.runExclusive(async (active) => {
      if (writeGeneration !== generation) {
        return
      }
      await active.clearInputLine()
      if (writeGeneration !== generation) {
        return
      }
      let restoredWrap = false
      if (responseStreaming) {
        await active.write('\x1B8')
        if (writeGeneration !== generation) {
          return
        }
        restoredWrap = responseWrapPending && !(options?.response
          && (text.startsWith('\n') || text.startsWith('\r\n')))
        responseWrapPending = false
        if (restoredWrap) {
          await active.write('\r\n')
          if (writeGeneration !== generation) {
            return
          }
        }
        if (!options?.response && responseLineOpen && !restoredWrap) {
          await active.write('\r\n')
          if (writeGeneration !== generation) {
            return
          }
        }
      }

      await active.write(text)
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
          const activeTerminal = active.terminal
          responseWrapPending = Boolean(
            activeTerminal
            && activeTerminal.buffer.active.cursorX === activeTerminal.cols,
          )
          await active.write('\x1B7')
          if (writeGeneration !== generation) {
            return
          }
          if (responseLineOpen) {
            await active.write('\r\n')
            if (writeGeneration !== generation) {
              return
            }
          }
          await redrawInputLine(active)
          return
        }
        responseWrapPending = false
        if (responseLineOpen) {
          await active.write('\r\n')
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
      await redrawInputLine(active)
    }))
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
