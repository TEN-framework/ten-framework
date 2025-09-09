# Contributing to TEN Framework

Welcome to the TEN Framework community! We're excited to have you contribute to building the future of real-time conversational AI agents. This guide will help you get started with contributing to the project.

## 🌟 Ways to Contribute

We welcome all forms of contributions:

- **Code contributions**: Bug fixes, new features, performance improvements
- **Documentation**: Improving docs, tutorials, examples  
- **Bug reports**: Identifying and reporting issues
- **Feature requests**: Suggesting new functionality
- **Community support**: Helping others in discussions and issues
- **Extensions**: Creating new extensions for the ecosystem

## 📋 Before You Start

### Required Reading

1. Read our [Code of Conduct](./CLA.md) - all contributors must follow our community guidelines
2. Sign the [Contributor License Agreement (CLA)](../../CLA.md) - required for all contributions
3. Review the project [README](../../README.md) to understand the architecture

### Prerequisites

Make sure you have the required tools installed:

- **Docker / Docker Compose** (for development environment)
- **Node.js v20+** (for frontend development)  
- **Python 3.8+** (for AI agent development)
- **Go** (for backend server development)
- **Task** (task runner - install via `npm install -g @go-task/cli`)

## 🚀 Getting Started

### 1. Fork and Clone

```bash
# Fork the repository on GitHub first, then:
git clone https://github.com/YOUR_USERNAME/ten-framework.git
cd ten-framework
```

### 2. Set Up Development Environment

```bash
# Navigate to AI agents directory
cd ai_agents

# Copy environment template
cp .env.example .env

# Edit .env file with your API keys:
# - AGORA_APP_ID and AGORA_APP_CERTIFICATE
# - OpenAI, Deepgram, ElevenLabs API keys as needed

# Start development containers
docker compose up -d

# Enter the container
docker exec -it ten_agent_dev bash

# Build the agent (takes 5-8 minutes)
task use

# Build and run
task build
task run
```

### 3. Development Workflow

The project uses Task as the build system. Key commands:

```bash
# Core development commands
task build          # Build the project
task test           # Run all tests
task lint           # Run linting
task format         # Format code
task clean          # Clean build artifacts

# AI Agents specific
task use AGENT=agents/examples/demo    # Switch to different agent
task run-server     # Run backend server
task run-gd-server  # Run graph designer
```

## 🏗️ Project Structure

```
ten-framework/
├── ai_agents/              # AI Agent implementations
│   ├── agents/             # Agent examples and configurations
│   ├── playground/         # Frontend playground (Next.js)
│   ├── server/             # Backend server (Go)
│   └── Taskfile.yml        # Build configuration
├── core/                   # TEN Framework core
├── packages/               # Framework packages
├── docs/                   # Documentation
├── tests/                  # Test suites
└── third_party/            # Third-party dependencies
```

## 🔧 Making Changes

### Code Guidelines

1. **Follow existing patterns**: Review similar code before implementing
2. **Use existing libraries**: Check if the codebase already uses needed dependencies
3. **Security first**: Never expose secrets or credentials
4. **No hardcoded comments**: Don't add unnecessary code comments unless requested

### Language-Specific Guidelines

**Python (AI Agents)**
- Follow PEP 8 style guide
- Use Black formatter: `task format`
- Run pylint: `task lint`
- Add type hints where appropriate

**TypeScript/JavaScript (Playground)**
- Use existing ESLint configuration
- Follow React best practices
- Use TypeScript for new code

**Go (Server)**
- Follow Go conventions
- Use gofmt for formatting
- Add proper error handling

### Commit Guidelines

We use conventional commits:

```bash
feat: add new ASR integration
fix: resolve memory leak in audio processing  
docs: update installation instructions
test: add integration tests for TTS module
refactor: improve error handling in server
```

## 🧪 Testing

### Running Tests

```bash
# Run all tests
task test

# Test specific extension
task test-extension EXTENSION=agents/ten_packages/extension/elevenlabs_tts_python

# Test server only
task test-server

# Test agent extensions
task test-agent-extensions
```

### Writing Tests

- Add unit tests for new functionality
- Include integration tests for complex features
- Ensure tests pass before submitting PRs
- Test configurations are in `tests/configs/` directories

## 📝 Documentation

- Update documentation for new features
- Include code examples where helpful
- Update README if adding new dependencies
- Document breaking changes clearly

## 🔄 Pull Request Process

### Before Submitting

1. **Test your changes**: Ensure all tests pass
2. **Format code**: Run `task format` 
3. **Lint code**: Run `task lint`
4. **Update docs**: Include any necessary documentation updates
5. **Rebase on main**: Ensure your branch is up to date

### PR Guidelines

1. **Clear title**: Use descriptive, conventional commit style
2. **Detailed description**: Explain what, why, and how
3. **Link issues**: Reference related issues with "Fixes #123"
4. **Screenshots**: Include for UI changes
5. **Breaking changes**: Clearly document any breaking changes

### PR Template

```markdown
## Summary
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature  
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring

## Testing
- [ ] Tests added/updated
- [ ] All tests pass
- [ ] Manual testing completed

## Documentation
- [ ] Documentation updated
- [ ] Examples provided if needed

## Breaking Changes
List any breaking changes and migration steps
```

## 🐛 Reporting Issues

### Bug Reports

Use the issue template and include:

- **Environment**: OS, versions, Docker info
- **Steps to reproduce**: Clear, minimal reproduction steps
- **Expected vs actual**: What should happen vs what happens
- **Logs**: Include relevant error messages
- **Screenshots**: For UI issues

### Feature Requests

- **Use case**: Explain the problem you're solving
- **Proposed solution**: Describe your ideal solution
- **Alternatives**: Consider other approaches
- **Examples**: Reference similar implementations if possible

## 🏆 Recognition

Contributors are recognized in multiple ways:

- **Contributors graph**: Automatic GitHub recognition
- **Release notes**: Significant contributions mentioned
- **Social media**: Major features highlighted on [@TenFramework](https://twitter.com/TenFramework)
- **Community showcase**: Featured implementations

## 📞 Getting Help

### Community Channels

- **Discord**: [Join TEN Community](https://discord.gg/VnPftUzAMJ)
- **GitHub Discussions**: For questions and ideas
- **GitHub Issues**: For bugs and feature requests
- **WeChat**: Chinese community group

### Maintainer Contact

- **Twitter**: [@elliotchen100](https://x.com/elliotchen100)
- **GitHub**: [@cyfyifanchen](https://github.com/cyfyifanchen)

## 📜 License

By contributing to TEN Framework, you agree that your contributions will be licensed under the [Apache License v2.0 with additional conditions](../../LICENSE).

Your contributions must:
- Be your original work or properly attributed
- Not include any proprietary or confidential information
- Comply with the project's licensing terms

## ❤️ Thank You

Every contribution, no matter how small, helps make TEN Framework better for everyone. We appreciate your time and effort in making this project successful!

**Happy Contributing!** 🚀