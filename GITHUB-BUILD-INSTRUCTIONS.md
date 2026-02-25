# 🚀 Build Redshift ODBC MSI on GitHub Actions

## ✨ Overview

Build the Redshift ODBC Driver MSI **automatically on GitHub** using GitHub Actions!
No need for local Windows machine!

---

## 🎯 Benefits

✅ **No Windows Machine Needed**
- GitHub provides free Windows runners
- Build in the cloud automatically

✅ **Automated Building**
- Push code → MSI built automatically
- Create tag → Release created automatically

✅ **Free for Public Repositories**
- 2,000 minutes/month free on GitHub
- Build time: ~15-20 minutes

✅ **Reproducible Builds**
- Same environment every time
- Documented build process

---

## 📋 Step 1: Create GitHub Repository

### Option A: Create New Repository

1. Go to https://github.com/new

2. Fill in:
   - **Repository name**: `redshift-odbc-fixed`
   - **Description**: `Redshift ODBC Driver v2.1.15.0 with authentication cancel fix`
   - **Visibility**: Public (for free builds) or Private

3. **DO NOT** initialize with README/gitignore/license

4. Click "Create repository"

### Option B: Use Existing Repository

If you already have a repository, you can use it.

---

## 📤 Step 2: Upload Code to GitHub

### Prepare the code:

```bash
# Navigate to the driver source
cd /home/orel/redshift-odbc-fix/amazon-redshift-odbc-driver

# Initialize git (if not already)
git init

# Add all files
git add .

# Commit
git commit -m "Redshift ODBC v2.1.15.0 with authentication cancel fix

- Fixed silent failure when user cancels Azure AD login
- Added empty authorization code validation
- Improved error messages
- Version bumped to 2.1.15.0"
```

### Push to GitHub:

```bash
# Add remote (replace with your repository URL)
git remote add origin https://github.com/YOUR-USERNAME/redshift-odbc-fixed.git

# Push
git push -u origin main
```

**Note**: Replace `YOUR-USERNAME` with your actual GitHub username.

---

## 🏗️ Step 3: GitHub Actions Will Build Automatically

Once you push, GitHub Actions will:

1. ✅ **Detect** the `.github/workflows/build-msi.yml` file
2. ✅ **Start** a Windows 2019 runner
3. ✅ **Install** Visual Studio, CMake, WiX Toolset
4. ✅ **Build** the driver
5. ✅ **Create** MSI installer
6. ✅ **Upload** MSI as artifact

### Monitor the build:

1. Go to your repository on GitHub
2. Click **"Actions"** tab
3. You'll see the build running
4. Click on the build to see progress
5. Wait ~15-20 minutes

---

## 📥 Step 4: Download the MSI

### Download from Build Artifacts:

1. Go to **Actions** tab
2. Click on the completed build
3. Scroll down to **"Artifacts"** section
4. Download **"redshift-odbc-msi"** (ZIP file)
5. Extract ZIP → Get MSI file

### Files in artifact:

```
redshift-odbc-msi.zip
├── AmazonRedshiftODBC64-2.1.15.0.msi
└── AmazonRedshiftODBC64-2.1.15.0.msi.sha256
```

---

## 🏷️ Step 5: Create Release (Optional)

To create an official GitHub Release with the MSI:

### Method 1: Push a Tag

```bash
# Create and push tag
git tag -a v2.1.15.0 -m "Release v2.1.15.0 - Authentication cancel fix"
git push origin v2.1.15.0
```

GitHub Actions will automatically:
- Build the MSI
- Create a GitHub Release
- Attach MSI to the release

### Method 2: Manual Release

1. Go to repository → **Releases**
2. Click **"Draft a new release"**
3. Choose tag: `v2.1.15.0` (create new)
4. Title: `Redshift ODBC Driver v2.1.15.0`
5. Description:
   ```markdown
   ## What's Fixed
   - Authentication cancellation now properly reports an error
   - User canceling Azure AD login shows immediate clear error
   - Closing browser during authentication shows immediate error

   ## Installation
   1. Download the MSI file below
   2. Double-click to install
   3. Configure ODBC DSN with BrowserAzureADOAuth2
   ```
6. Upload MSI manually (download from artifacts first)
7. Click **"Publish release"**

---

## 🧪 Step 6: Test the MSI

After downloading:

1. **Install on Windows**:
   ```cmd
   msiexec /i AmazonRedshiftODBC64-2.1.15.0.msi
   ```

2. **Configure ODBC DSN**:
   - Open "ODBC Data Source Administrator (64-bit)"
   - Add → Amazon Redshift (x64)
   - plugin_name: BrowserAzureADOAuth2
   - Configure other settings

3. **Test Authentication Cancel**:
   - Click "Test"
   - Browser opens → Click "Cancel"
   - ✅ Should show immediate error:
     ```
     Authentication failed. The authorization code was not received.
     This may occur if you cancelled the login...
     ```

4. **Test Browser Close**:
   - Click "Test"
   - Browser opens → Close window (X)
   - ✅ Should show same error

5. **Test Successful Login**:
   - Click "Test"
   - Complete Azure AD login
   - ✅ Should show "Connection Successful"

---

## 🔧 Workflow Configuration

The GitHub Actions workflow (`.github/workflows/build-msi.yml`) includes:

### Triggers:
- **Push** to main/master/fix-auth-cancel branches
- **Pull Request** to main/master
- **Tag** push (v*)
- **Manual** dispatch (click "Run workflow" button)

### Steps:
1. Checkout code
2. Setup MSBuild (Visual Studio)
3. Setup CMake
4. Install WiX Toolset (for MSI creation)
5. Verify all build tools
6. Configure CMake
7. Build driver (parallel build)
8. Install driver
9. Find and verify MSI
10. Calculate SHA256 checksum
11. Upload as artifact
12. Create release (if tag)

### Environment:
- **OS**: Windows Server 2019
- **CMake**: Latest
- **Visual Studio**: 2019
- **WiX Toolset**: 3.11

---

## 📊 Build Status

You can add a build badge to your README:

```markdown
![Build Status](https://github.com/YOUR-USERNAME/redshift-odbc-fixed/workflows/Build%20Redshift%20ODBC%20MSI/badge.svg)
```

---

## 💰 Cost

### Free Tier (Public Repository):
- ✅ **2,000 minutes/month** free
- ✅ Build time: ~15-20 minutes
- ✅ **~100-130 builds/month** for free!

### Private Repository:
- Free tier: 2,000 minutes/month (on free plan)
- After: $0.008/minute
- Build cost: ~$0.12-0.16 per build

**Recommendation**: Use public repository for free unlimited builds!

---

## 🐛 Troubleshooting

### Build Fails

**Check**:
1. `.github/workflows/build-msi.yml` is in the repository
2. File is valid YAML (indentation matters!)
3. Branch name matches trigger (main/master)

**View Logs**:
1. Go to Actions tab
2. Click on failed build
3. Click on failed step
4. Read error message

### WiX Toolset Not Found

**Issue**: MSI not created but build succeeded

**Solution**: The workflow installs WiX automatically via chocolatey.
If it fails, check the "Install WiX Toolset" step logs.

### CMake Configuration Error

**Issue**: CMake fails to configure

**Solution**: Check that CMakeLists.txt is in repository root.

---

## 📁 Repository Structure

Your repository should look like this:

```
redshift-odbc-fixed/
├── .github/
│   └── workflows/
│       └── build-msi.yml          ← GitHub Actions workflow
├── src/
│   └── odbc/
│       └── rsodbc/
│           └── iam/
│               └── plugins/
│                   └── IAMBrowserAzureOAuth2CredentialsProvider.cpp ← Fixed
├── CMakeLists.txt
├── version.txt                     ← 2.1.15.0
├── build64.bat
├── build64.sh
├── README.md
└── ... (other files)
```

---

## 🎯 Quick Start Summary

1. **Create GitHub repository**
2. **Upload code**:
   ```bash
   cd /home/orel/redshift-odbc-fix/amazon-redshift-odbc-driver
   git init
   git add .
   git commit -m "Redshift ODBC v2.1.15.0 with auth cancel fix"
   git remote add origin https://github.com/YOUR-USERNAME/redshift-odbc-fixed.git
   git push -u origin main
   ```
3. **Wait** ~15-20 minutes for build
4. **Download** MSI from Actions → Artifacts
5. **Test** on Windows
6. **Done**!

---

## 🔄 Rebuild

To trigger a new build:

### Method 1: Push Changes
```bash
git commit --allow-empty -m "Rebuild"
git push
```

### Method 2: Manual Dispatch
1. Go to Actions tab
2. Click "Build Redshift ODBC MSI"
3. Click "Run workflow"
4. Select branch
5. Click "Run workflow" button

---

## 📞 Support

### GitHub Actions Issues:
- Check Actions tab for build logs
- Read error messages carefully
- Verify YAML syntax

### Build Issues:
- Same as local Windows build
- Check build logs in Actions
- MSI should be ~7-8 MB

### Questions:
- Check GitHub Actions documentation
- Review workflow YAML file
- Look at build logs

---

## ✅ Advantages Over Local Build

| Feature | Local Build | GitHub Actions |
|---------|-------------|----------------|
| Requires Windows machine | ✅ Yes | ❌ No |
| Requires Visual Studio | ✅ Yes | ❌ No |
| Requires CMake | ✅ Yes | ❌ No |
| Requires WiX Toolset | ✅ Yes | ❌ No |
| Setup time | 1-2 hours | 0 minutes |
| Build time | 15-25 min | 15-20 min |
| Cost | Hardware | Free |
| Reproducible | ⚠️ Maybe | ✅ Always |
| Automatic | ❌ Manual | ✅ Automatic |

---

## 🎉 Success!

Once you have the MSI from GitHub Actions:

✅ **You built a Windows driver without Windows!**
✅ **You have a reproducible build process**
✅ **You can rebuild anytime for free**
✅ **You can share the MSI with users**

---

**Version**: 2.1.15.0
**Platform**: GitHub Actions (Windows 2019)
**Build Time**: ~15-20 minutes

🤖 Generated with [Claude Code](https://claude.com/claude-code)
