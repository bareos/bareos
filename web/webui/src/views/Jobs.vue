<template>
  <section>
    <div class="box">
      <job-listing v-on:job-selected="selectedJob = $event"/>
    </div>
    <div class="box" v-if="selectedJob">
      <job-log :jobid="selectedJob"></job-log>
    </div>
  </section>
</template>

<script>
import JobListing from '../components/JobListing'
import JobDetails from '../components/JobDetails'
import JobLog from '../components/JobLog'

export default {
  name: 'Jobs',
  components: {
    JobLog,
    JobListing
  },
  data () {
    return {
      selectedJob: 0
    }
  },
  methods: {}
}
</script>

<style scoped>

</style>
