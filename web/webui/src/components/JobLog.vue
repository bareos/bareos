<template>
  <section>
    <table class="table is-striped">
      <thead>
      <th>Timestamp</th>
      <th>Log</th>
      </thead>
      <template v-for="log in jobLog">
        <tr :key="log.time">
          <td> <span class="is-size-8">{{log.time}}</span></td>
          <td><pre>{{log.logtext}}</pre></td>
        </tr>
      </template>
    </table>
  </section>
</template>

<script>
import { getJobLog } from '@/models/jobs'

export default {
  name: 'JobLog',
  props: {
    jobid: Number
  },
  data () {
    return {
      job: Object,
      jobLog: Object
    }
  },
  computed: {
    formatTimeStamp: function (date) {
      // todo: date formate here
      return date
    }
  },
  watch: {
    jobid: async function (val) {
      // todo: check result and catch exceptions
      this.jobLog = await getJobLog(this.$http, val)
    }
  }
}
</script>

<style scoped>

</style>
