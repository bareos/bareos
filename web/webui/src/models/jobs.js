
// todo: replace \n by <br /> or wrap into <p></p>

export async function getJobs (http) {
  console.log('getJobs')
  try {
    const response = await http.get(`/command/llist/jobs`)
    return response.data.jobs
  } catch (e) {
    console.warn(e)
  }
  return null
}

export async function getJob (http, jobid) {
  console.log('getJob')
  try {
    const response = await http.get(`/command/llist/job/${jobid}`)
    return response.data.jobs
  } catch (e) {
    console.warn(e)
  }
  return null
}

export async function getJobLog (http, jobid) {
  console.log('getJoblog')
  try {
    const response = await http.get(`/command/llist/joblog/${jobid}`)
    return response.data.joblog
  } catch (e) {
    console.warn(e)
  }
  return null
}

export const jobStatus = new Map([
  ['C', 'Created, not yet running'],
  ['R', 'Running'],
  ['B', 'Blocked'],
  ['T', 'Completed successfully'],
  ['E', 'Terminated with errors'],
  ['e', 'Non-fatal error'],
  ['f', 'Fatal error'],
  ['D', 'Verify found diﬀerences'],
  ['A', 'Canceled by user'],
  ['I', 'Incomplete job'],
  ['L', 'Committing data'],
  ['W', 'Terminated with warnings'],
  ['l', 'Doing data despooling'],
  ['q', 'Queued waiting for device'],
  ['F', 'Waiting for Client'],
  ['S', 'Waitin:g for Storage daemon'],
  ['m', 'Waiting for new media'],
  ['M', 'Waiting for media mount'],
  ['s', 'Waiting for storage resource'],
  ['j', 'Waiting for job resource'],
  ['c', 'Waiting for client resource'],
  ['d', 'Waiting on maximum jobs'],
  ['t', 'Waiting on start time'],
  ['p', 'Waiting on higher priority jobs'],
  ['i', 'Doing batch insert ﬁle records'],
  ['a', 'SD despooling attributes']
])

export const jobTypes = new Map([
  ['B', 'Backup'],
  ['M', 'Migrated'],
  ['V', 'Verify'],
  ['R', 'Restore'],
  ['U', 'Console program'],
  ['I', 'Internal system job'],
  ['D', 'Admin'],
  ['A', 'Archive'],
  ['C', 'Copy of a job'],
  ['c', 'Copy job'],
  ['g', 'Migration job'],
  ['S', 'Scan'],
  ['O', 'Consolidate']
])

export const jobLevels = new Map([
  ['F', 'Full'],
  ['D', 'Differential'],
  ['I', 'Incremental'],
  ['f', 'VirtualFull'],
  ['B', 'Base'],
  ['C', 'Catalog'],
  ['V', 'InitCatalog'],
  ['O', 'VolumeToCatalog'],
  ['d', 'DiskToCatalog'],
  ['A', 'Data'],
  [' ', 'None']
])
