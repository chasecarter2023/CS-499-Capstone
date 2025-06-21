import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

// Interface representing the expected response from the login endpoint.
export interface LoginResponse {
  token: string;
}

@Injectable({
  providedIn: 'root'
})
export class AuthService {
  // Ensure this URL matches your back-end login route
  private loginUrl = 'http://127.0.0.1:3000/API/login';

  constructor(private http: HttpClient) {}

  // Method that sends user credentials to the back-end and returns an Observable of the LoginResponse.
  login(credentials: { username: string; password: string }): Observable<LoginResponse> {
    return this.http.post<LoginResponse>(this.loginUrl, credentials);
  }
}
