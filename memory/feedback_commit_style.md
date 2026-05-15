---
name: feedback_commit_style
description: User wants only a commit message suggestion, not for Claude to run git commands
metadata:
  type: feedback
---

When asked to "commit", just suggest a commit message — do not run git commands.

**Why:** User prefers to run git themselves; running the commit without explicit approval is too proactive.

**How to apply:** On any "commit" request, output the message text only. Only run git if the user explicitly says "go ahead" or similar.
