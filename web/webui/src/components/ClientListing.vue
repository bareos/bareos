<template>
  <div>
  <v-data-table
    :headers="headers"
    :items="clients[0]"
    class="elevation-1"
  >
    <template slot="items" slot-scope="props">
      <td>{{ props.item.clientid }}</td>
      <td class="text-xs-right">{{ props.item.name }}</td>
      <td class="text-xs-right">{{ props.item.uname }}</td>
      <td class="text-xs-right">{{ props.item.fileretention }}</td>
      <td class="text-xs-right">{{ props.item.jobretention }}</td>
      <td class="text-xs-right">{{ props.item.autoprune }}</td>
    </template>
  </v-data-table>
  </div>
</template>

<script>
import { getClients } from '@/models/clients'

export default {
  name: 'ClientListing',
  data() {
    return {
      clients: [],
      headers: [
        { text: 'clientid', value: 'clientid' },
        { text: 'name', value: 'name' },
        { text: 'uname', value: 'uname' },
        { text: 'fileretention', value: 'fileretention' },
        { text: 'jobretention', value: 'jobretention' },
        { text: 'autoprune', value: 'autoprune' },
      ],
    }
  },
  mounted() {
    getClients(this.$socket)
  },
  created() {
    this.$socket.onmessage = (data) => {
      const x = JSON.parse(data.data)
      this.clients.push(x.result.clients)
    }
    // console.log(this.clients)
  },
  methods: {
    getClients(ws) {
      this.clients = []
      ws.send('llist clients')
    },
  },
  watch: {
  },
  computed: {
  },
}
</script>

<style>
</style>
