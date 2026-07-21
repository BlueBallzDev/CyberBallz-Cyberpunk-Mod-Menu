"""YouTube Data API v3 upload (videos.insert + thumbnails.set)."""

from pathlib import Path

from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload

SCOPES = ["https://www.googleapis.com/auth/youtube.upload"]


def run_auth_flow(client_secret: Path, token_file: Path) -> None:
    """One-time interactive OAuth per channel. Needs a browser; run on a
    desktop, then copy the token file to the machine that does the uploading.

    Sign in with the Google account that owns the target channel — the token
    determines which channel receives the uploads."""
    if not client_secret.exists():
        raise SystemExit(
            f"Missing {client_secret}. Create an OAuth client (Desktop app) in "
            "Google Cloud Console and download it there. See README.md."
        )
    flow = InstalledAppFlow.from_client_secrets_file(str(client_secret), SCOPES)
    creds = flow.run_local_server(port=0)
    token_file.parent.mkdir(parents=True, exist_ok=True)
    token_file.write_text(creds.to_json())
    print(f"Saved credentials to {token_file}")


def get_service(token_file: Path):
    if not token_file.exists():
        raise SystemExit(
            f"Not authenticated ({token_file} missing). Run: python run.py auth"
        )
    creds = Credentials.from_authorized_user_file(str(token_file), SCOPES)
    if creds.expired and creds.refresh_token:
        creds.refresh(Request())
        token_file.write_text(creds.to_json())
    return build("youtube", "v3", credentials=creds)


def upload_video(
    video_path: Path,
    token_file: Path,
    title: str,
    description: str,
    tags: list[str],
    category_id: str,
    privacy: str,
    publish_at: str | None,
    notify_subscribers: bool,
    thumbnail_path: Path | None = None,
) -> str:
    service = get_service(token_file)

    status: dict = {
        "privacyStatus": privacy,
        "selfDeclaredMadeForKids": False,
    }
    if publish_at:
        status["privacyStatus"] = "private"
        status["publishAt"] = publish_at

    body = {
        "snippet": {
            "title": title,
            "description": description,
            "tags": tags,
            "categoryId": category_id,
        },
        "status": status,
    }

    media = MediaFileUpload(str(video_path), mimetype="video/mp4",
                            chunksize=-1, resumable=True)
    request = service.videos().insert(
        part="snippet,status",
        body=body,
        media_body=media,
        notifySubscribers=notify_subscribers,
    )

    response = None
    while response is None:
        progress, response = request.next_chunk()
        if progress:
            print(f"  upload {int(progress.progress() * 100)}%")
    video_id = response["id"]
    print(f"  uploaded: https://youtu.be/{video_id}")

    if thumbnail_path is not None:
        # Custom thumbnails need a phone-verified channel; don't fail the run over it.
        try:
            service.thumbnails().set(
                videoId=video_id,
                media_body=MediaFileUpload(str(thumbnail_path), mimetype="image/png"),
            ).execute()
            print("  thumbnail set")
        except Exception as e:
            print(f"  thumbnail failed (channel not verified for custom thumbnails?): {e}")

    return video_id
