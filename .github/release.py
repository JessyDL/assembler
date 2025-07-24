import argparse
import subprocess
import sys

def get_git_sha1():
    try:
        sha1 = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
        return sha1
    except subprocess.CalledProcessError as e:
        print(f"Error getting git SHA1: {e}", file=sys.stderr)
        sys.exit(1)

def get_current_branch():
    try:
        branch = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], text=True).strip()
        return branch
    except subprocess.CalledProcessError as e:
        print(f"Error getting current branch: {e}", file=sys.stderr)
        sys.exit(1)

def get_last_version_tag():
    try:
        tags = subprocess.check_output(['git', 'tag'], text=True).strip().split('\n')
        # Only consider tags matching release/X.Y.Z
        version_tags = []
        for t in tags:
            m = re.match(r'release/(\d+)\.(\d+)\.(\d+)$', t)
            if m:
                version_tags.append((t, tuple(int(x) for x in m.groups())))
        if not version_tags:
            print("No tags found.", file=sys.stderr)
            sys.exit(1)
        last_tag = sorted(version_tags, key=lambda x: x[1], reverse=True)[0][0]
        m = re.match(r'release/(\d+\.\d+\.\d+)$', last_tag)
        if m:
            return m.group(1)
        else:
            raise ValueError("No valid version tag found.")
    except subprocess.CalledProcessError as e:
        print(f"Error getting last version tag: {e}", file=sys.stderr)
        sys.exit(1)

def push_release_tag(version):
    if not isinstance(version, str) or not version.count('.') == 2 or not all(part.isdigit() for part in version.split('.')):
        print("Error: Version must be in the format 'X.Y.Z' where X, Y, Z are integers.", file=sys.stderr)
        sys.exit(1)
    branch = get_current_branch()
    if branch != "develop":
        print("Error: You must be on the 'develop' branch to push a release tag.", file=sys.stderr)
        sys.exit(1)
    tag_name = f"release/{version}"
    try:
        subprocess.check_call(['git', 'tag', '-f', tag_name])
        subprocess.check_call(['git', 'push', 'origin', f'refs/tags/{tag_name}'])
        print(f"Pushed tag {tag_name} to origin.")
    except subprocess.CalledProcessError as e:
        print(f"Error pushing release tag: {e}", file=sys.stderr)
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Git release utility script.")
    group = parser.add_argument_group("output options")
    group.add_argument('--sha1', action='store_true', help="Output the current git SHA1.")
    group.add_argument('--version', action='store_true', help="Output the current associated version given the last tag.")
    parser.add_argument('--release', metavar='VERSION', help="Push the current develop branch to release/<version> tag.", action='store')
    args = parser.parse_args()

    if args.release and (args.sha1 or args.version):
        print("Error: --release cannot be combined with --sha1 or --version.", file=sys.stderr)
        sys.exit(1)

    output = ""
    if args.version:
        output = get_last_version_tag()
    if args.sha1:
        output = f"{output}-{get_git_sha1()}" if output else get_git_sha1()

    if output:
        print(output)

    if args.release:
        push_release_tag(args.release)

if __name__ == "__main__":
    main()
