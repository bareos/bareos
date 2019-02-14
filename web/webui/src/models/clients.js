export function getClients (ws) {
  ws.send('.api 2')
  ws.send('llist clients')
}
