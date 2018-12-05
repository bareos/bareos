<template>
  <table class="table is-striped">
    <tr>
      <td>Name:</td>
      <td>{{job.name}}</td>
    </tr>
    <tr>
      <td>Job ID:</td>
      <td>{{job.jobid}}</td>
    </tr>
  </table>
</template>

<script>
// {
//   "jobid": "2",
//   "job": "backup-bareos-fd.2018-12-05_12.14.29_08",
//   "name": "backup-bareos-fd",
//   "purgedfiles": "0",
//   "type": "B",
//   "level": "F",
//   "clientid": "1",
//   "client": "localhost-fd",
//   "jobstatus": "T",
//   "schedtime": "2018-12-05 12:14:27",
//   "starttime": "2018-12-05 12:14:30",
//   "endtime": "2018-12-05 12:14:30",
//   "realendtime": "2018-12-05 12:14:30",
//   "jobtdate": "1544008470",
//   "volsessionid": "2",
//   "volsessiontime": "1544004635",
//   "jobfiles": "188",
//   "jobbytes": "8264705",
//   "joberrors": "0",
//   "jobmissingfiles": "0",
//   "poolid": "3",
//   "poolname": "Full",
//   "priorjobid": "0",
//   "filesetid": "2",
//   "fileset": "SelfTest"
// }

import { getJob } from '@/models/jobs'

export default {
  name: 'JobDetails',
  props: {
    jobid: Number
  },
  data () {
    return {
      job: Object,
      jobLog: Object
    }
  },
  watch: {
    jobid: async function (val) {
      const jobs = await getJob(this.$http, val)
      // todo: validate result and catch exceptions
      this.job = jobs[0]
    }
  }
}
</script>

<style scoped>

</style>
