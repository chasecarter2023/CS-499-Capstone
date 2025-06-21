// src/app/components/login/login.component.ts
import { Component, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, ReactiveFormsModule } from '@angular/forms';
import { Router, RouterModule } from '@angular/router';
import { AuthService } from '../../services/auth.service';
import { CommonModule } from '@angular/common';

@Component({
  selector: 'app-login',
  standalone: true,                     // Make this a standalone component
  imports: [CommonModule, ReactiveFormsModule, RouterModule], // Import necessary modules
  templateUrl: './login.component.html',
  styleUrls: ['./login.component.css']  // Optional styling file if you have one
})
export class LoginComponent implements OnInit {
  loginForm!: FormGroup;

  constructor(
    private fb: FormBuilder, 
    private authService: AuthService, 
    private router: Router
  ) {}

  ngOnInit(): void {
    // Initialize the reactive form
    this.loginForm = this.fb.group({
      username: [''],
      password: ['']
    });
  }

  onLogin(): void {
    const credentials = this.loginForm.value;
    this.authService.login(credentials).subscribe({
      next: (res) => {
        // Save the JWT token in localStorage
        localStorage.setItem('token', res.token);
        // Redirect to a protected route, for example, your trips list/dashboard
        this.router.navigate(['/trips']);
      },
      error: (err) => {
        console.error('Login failed', err);
      }
    });
  }
}
