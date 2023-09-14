#include "jsonrpcrestore.h"

#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/bsr.h"
#include "dird/director_jcr_impl.h"
#include "dird/ua_tree.h"
#include "dird/ua_restore.h"
#include "dird/ua_input.h"
#include "dird/ua_run.h"
#include "lib/tree.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"

namespace directordaemon {

static Json::Value RPCFinishSelection(RestoreResources& restore_resources)
{
  Json::Value result;
  UaContext* ua = restore_resources.manager.ua_;

  FinishSelection(&restore_resources.rx, restore_resources.tree);

  /* We keep the tree with selected restore files.
   * For NDMP restores its used in the DMA to know what to restore.
   * The tree is freed by the DMA when its done. */
  ua->jcr->dir_impl->restore_tree_root = restore_resources.tree.root;

  restore_resources.state = RestoreState::end_select_files;

  if (restore_resources.rx.bsr->JobId) {
    if (!AddVolumeInformationToBsr(ua, restore_resources.rx.bsr.get())) {
      result["error"] = "cannot construct bootstrap";
      return result;
    }
    if (!(restore_resources.rx.selected_files
          = WriteBsrFile(ua, restore_resources.rx))) {
      result["selectedfiles"] = 0;
      return result;
    }

  } else {
    result["selectedfiles"] = 0;
    return result;
  }

  result["selectedfiles"] = restore_resources.rx.selected_files;
  return result;
}

static Json::Value RestoreRunCmd(UaContext* ua, RestoreContext& rx)
{
  std::string command_to_run = BuildRestoreCommandString(
      rx, ua->catalog->resource_name_, ua->jcr->RestoreBootstrap, true);
  Mmsg(ua->cmd, command_to_run.c_str());

  ua->jcr->JobIds = rx.JobIds;

  ParseUaArgs(ua);
  uint32_t restore_jobid = DoRunCmd(ua);

  Json::Value result;
  result["jobid"] = restore_jobid;
  result["commmand run"] = std::string(ua->cmd);

  return result;
}

Json::Value InititiateRestore(const std::string& clientname,
                              RestoreResources& restore_resources,
                              int jobid)
{
  restore_resources.rx.JobIds.append(std::to_string(jobid));
  Json::Value result;
  std::vector<JobResource*> restorejobs = GetRestoreJobs();
  if (restorejobs.empty()) {
    result["error"] = "No Restore job resource found";
    return result;
  } else {
    restore_resources.rx.restore_job = restorejobs.front();
  }

  restore_resources.tree = BuildDirectoryTree(restore_resources.manager.ua_,
                                              &restore_resources.rx);

  if (restore_resources.tree.FileCount == 0) {
    result["error"] = "For the jobid selected, no files were found";
    return result;
  }

  restore_resources.tree.node = (TREE_NODE*)restore_resources.tree.root;
  std::string cwd{tree_getpath(restore_resources.tree.node)};

  restore_resources.rx.ClientName = clientname;
  restore_resources.rx.RestoreClientName = clientname;

  result["Restorable files"] = restore_resources.tree.FileCount;
  result["current directory"] = cwd;

  restore_resources.state = RestoreState::start_select_files;
  return result;
}

Json::Value CdCmd(const std::string& path, TreeContext* tree)
{
  Json::Value result;
  std::string pathcopy(path);

  if (path.empty()) {
    result["error"] = "Empty path";
    return result;
  }

  POOLMEM* cwd;
  TREE_NODE* node = tree_cwd(pathcopy.data(), tree->root, tree->node);
  if (!node) {
    // Try once more if Win32 drive -- make absolute
    if (path[1] == ':') { /* win32 drive */
      cwd = GetPoolMemory(PM_FNAME);
      PmStrcpy(cwd, "/");
      PmStrcat(cwd, path.c_str());
      node = tree_cwd(cwd, tree->root, tree->node);
      FreePoolMemory(cwd);
    }

    if (!node) {
      result["warning"] = "Invalid path given";
    } else {
      tree->node = node;
    }
  } else {
    tree->node = node;
  }

  result["current directory"] = std::string(tree_getpath(tree->node));
  return result;
}

Json::Value LsCmd(int limit,
                  int offset,
                  const std::string& path,
                  TreeContext* tree)
{
  Json::Value result;
  Json::Value filelist;
  std::string pathcopy(path);
  TREE_NODE* ls_node = nullptr;
  if (!path.empty()) {
    POOLMEM* cwd;
    ls_node = tree_cwd(pathcopy.data(), tree->root, tree->node);
    if (!ls_node) {
      // Try once more if Win32 drive -- make absolute
      if (path[1] == ':') { /* win32 drive */
        cwd = GetPoolMemory(PM_FNAME);
        PmStrcpy(cwd, "/");
        PmStrcat(cwd, path.c_str());
        ls_node = tree_cwd(cwd, tree->root, tree->node);
        FreePoolMemory(cwd);
      }

      if (!ls_node) {
        result["error"] = "Invalid path given";
        return result;
      }
    }
  }

  if (!ls_node) { ls_node = tree->node; }

  int count = 0;
  if (TreeNodeHasChild(ls_node)) {
    TREE_NODE* node;
    foreach_child (node, ls_node) {
      if (count >= limit) { break; }
      if (count < offset) {
        count++;
        continue;
      }
      const char* tag;
      if (node->extract) {
        tag = "*";
      } else if (node->extract_dir) {
        tag = "+";
      } else {
        tag = "";
      }

      const char* filename_ending = TreeNodeHasChild(node) ? "/" : "";
      std::string filename = std::string(node->fname) + filename_ending;

      std::string filetype;
      switch (node->type) {
        case TreeNodeType::FILE:
          filetype = "File";
          break;
        case TreeNodeType::DIR:
          filetype = "Directory";
          break;
        case TreeNodeType::DIR_NLS:
          filetype = "Directory(windows)";
          break;
        case TreeNodeType::ROOT:
          filetype = "Root node";
          break;
        case TreeNodeType::NEWDIR:
          filetype = "New Directory";
          break;
        case TreeNodeType::UNKNOWN:
          filetype = "Unkown file type";
          break;
      }

      Json::Value file;
      file["fileindex"] = node->FileIndex;
      file["name"] = filename;
      file["tag"] = tag;
      file["type"] = filetype;

      filelist.append(file);

      count++;
    }
  }

  result["file list"] = filelist;
  return result;
}

Json::Value LsCount(const std::string& path, TreeContext* tree)
{
  Json::Value result;
  std::string pathcopy(path);
  TREE_NODE* ls_node = nullptr;
  if (!path.empty()) {
    POOLMEM* cwd;
    ls_node = tree_cwd(pathcopy.data(), tree->root, tree->node);
    if (!ls_node) {
      // Try once more if Win32 drive -- make absolute
      if (path[1] == ':') { /* win32 drive */
        cwd = GetPoolMemory(PM_FNAME);
        PmStrcpy(cwd, "/");
        PmStrcat(cwd, path.c_str());
        ls_node = tree_cwd(cwd, tree->root, tree->node);
        FreePoolMemory(cwd);
      }

      if (!ls_node) {
        result["error"] = "Invalid path given";
        return result;
      }
    }
  }

  if (!ls_node) { ls_node = tree->node; }

  int count = 0;
  if (TreeNodeHasChild(ls_node)) {
    TREE_NODE* node;
    foreach_child (node, ls_node) { count++; }
  }

  result["file count"] = count;
  return result;
}

Json::Value DoneCmd(RestoreResources& restore_resources)
{
  return RPCFinishSelection(restore_resources);
}

Json::Value CommitRestore(RestoreResources& restore_resources)
{
  Json::Value result;
  switch (restore_resources.state) {
    case RestoreState::initiate:
      result["error"] = "Restore did not start yet. Start a restore first.";
      break;

    case RestoreState::start_select_files:
      result["error"]
          = "Restore still in selection mode. Finish selection first.";
      break;

    case RestoreState::end_select_files:
      result
          = RestoreRunCmd(restore_resources.manager.ua_, restore_resources.rx);
      break;

    default:
      result["error"] = "Unkonwn restore state, cannot commit restore.";
      break;
  }

  return result;
}

Json::Value MarkCmd(UaContext* ua, TreeContext& tree, const Json::Value& files)
{
  Json::Value result;
  int count = 0;
  for (const auto& file : files) {
    count += MarkElement(file.asCString(), ua, &tree, true);
  }

  result["marked files"] = count;
  return result;
}

Json::Value UnmarkCmd(UaContext* ua,
                      TreeContext& tree,
                      const Json::Value& files)
{
  Json::Value result;
  int count = 0;
  for (const auto& file : files) {
    count += MarkElement(file.asCString(), ua, &tree, false);
  }

  result["unmarked files"] = count;
  return result;
}

}  // namespace directordaemon
