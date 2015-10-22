#ifndef R3_UTIL_CATKINRESOURCERETRIEVER_H_
#define R3_UTIL_CATKINRESOURCERETRIEVER_H_
#include <string>
#include <unordered_map>
#include <vector>
#include <dart/common/ResourceRetriever.h>

namespace r3 {
namespace util {

class CatkinResourceRetriever : public virtual dart::common::ResourceRetriever {
public:
  CatkinResourceRetriever();
  explicit CatkinResourceRetriever(
    const dart::common::ResourceRetrieverPtr& _delegate);
  virtual ~CatkinResourceRetriever() = default;

  bool exists(const dart::common::Uri& _uri) const override;

  dart::common::ResourcePtr retrieve(
    const dart::common::Uri& _uri) const override;

private:
  struct Workspace {
    std::string mPath;
    std::unordered_map<std::string, std::string> mSourceMap;
  };

  std::vector<Workspace> getWorkspaces() const;
  dart::common::Uri resolvePackageUri(const dart::common::Uri& _uri) const;

  dart::common::ResourceRetrieverPtr mDelegate;
  std::vector<Workspace> mWorkspaces;
};

} // namespace util
} // namespace r3

#endif // ifndef R3_UTIL_CATKINRESOURCERETRIEVER_H_